#include "PBSubsystem.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Net/PBHttpClient.h"
#include "PBModule.h"
#include "PBSettings.h"

void UProtoBoySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ObsidianConnector = MakeUnique<FProtoBoyObsidianConnector>();
	DocGenerator = MakeUnique<FProtoBoyDocGenerator>(*ObsidianConnector);
	DaemonClient = MakeUnique<FProtoBoyDaemonClient>();
	MCPServer = MakeUnique<FProtoBoyMCPServer>();
	TrelloConnector = MakeUnique<FProtoBoyTrelloConnector>();
	TrelloClient = MakeUnique<FProtoBoyTrelloClient>(*TrelloConnector);
	DocMap = MakeUnique<FProtoBoyDocMap>(*DocGenerator);
	DocsConnectionChangedHandle = ObsidianConnector->OnConnectionChanged.AddLambda([this]() { DocMap->Refresh(); });

	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (Settings->bConnectToDaemon)
	{
		DaemonClient->Connect(Settings->DaemonUrl);
	}
	if (Settings->bEnableMCPServer || MCPServer->HasConnectedClients())
	{
		MCPServer->Start(Settings->MCPPort);
	}

	BindEditorEvents();
	UE_LOG(LogProtoBoy, Log, TEXT("ProtoBoy subsystem initialized."));
}

void UProtoBoySubsystem::Deinitialize()
{
	UnbindEditorEvents();
	TrelloConnector->CancelConnect();
	MCPServer->Stop();
	DaemonClient->Disconnect();

	ObsidianConnector->OnConnectionChanged.Remove(DocsConnectionChangedHandle);
	DocMap.Reset();
	TrelloClient.Reset();
	TrelloConnector.Reset();
	MCPServer.Reset();
	DaemonClient.Reset();
	DocGenerator.Reset();
	ObsidianConnector.Reset();

	Super::Deinitialize();
}

UProtoBoySubsystem* UProtoBoySubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UProtoBoySubsystem>() : nullptr;
}

IProtoBoyTaskProvider* UProtoBoySubsystem::GetTaskProvider() const
{
	return TrelloConnector->IsConnected() ? TrelloClient.Get() : nullptr;
}

void UProtoBoySubsystem::BindEditorEvents()
{
	if (FSlateApplication::IsInitialized())
	{
		ModalLoopTickHandle = FSlateApplication::Get().GetOnModalLoopTickEvent().AddUObject(this, &UProtoBoySubsystem::OnModalLoopTick);
	}
	CoreTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UProtoBoySubsystem::OnCoreTick));

	// TODO: IAssetRegistry::OnAssetAdded/Removed/Renamed, UPackage::PackageSavedWithContextEvent,
	//       ILiveCodingModule::GetOnPatchCompleteDelegate.
}

void UProtoBoySubsystem::UnbindEditorEvents()
{
	if (ModalLoopTickHandle.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetOnModalLoopTickEvent().Remove(ModalLoopTickHandle);
	}
	ModalLoopTickHandle.Reset();

	if (CoreTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CoreTickHandle);
		CoreTickHandle.Reset();
	}

	// TODO: Remove the handles bound in BindEditorEvents.
}

bool UProtoBoySubsystem::OnCoreTick(float DeltaTime)
{
	ProtoBoyHttp::PumpGameThreadQueue();
	return true;
}

void UProtoBoySubsystem::OnModalLoopTick(float DeltaTime)
{
	ProtoBoyHttp::PumpGameThreadQueue();
	MCPServer->TickWhileModal(DeltaTime);
	TrelloConnector->TickWhileModal(DeltaTime);
}
