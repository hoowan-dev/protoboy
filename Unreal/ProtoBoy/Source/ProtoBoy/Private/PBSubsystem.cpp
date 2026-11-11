#include "PBSubsystem.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "PBModule.h"
#include "PBSettings.h"

void UProtoBoySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ObsidianConnector = MakeUnique<FProtoBoyObsidianConnector>();
	DocGenerator = MakeUnique<FProtoBoyDocGenerator>(*ObsidianConnector);
	HealthChecker = MakeUnique<FProtoBoyHealthChecker>();
	AssetLifecycle = MakeUnique<FProtoBoyAssetLifecycle>();
	DaemonClient = MakeUnique<FProtoBoyDaemonClient>();
	MCPServer = MakeUnique<FProtoBoyMCPServer>();
	TrelloConnector = MakeUnique<FProtoBoyTrelloConnector>();

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

	TrelloConnector.Reset();
	MCPServer.Reset();
	DaemonClient.Reset();
	AssetLifecycle.Reset();
	HealthChecker.Reset();
	DocGenerator.Reset();
	ObsidianConnector.Reset();

	Super::Deinitialize();
}

UProtoBoySubsystem* UProtoBoySubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UProtoBoySubsystem>() : nullptr;
}

void UProtoBoySubsystem::BindEditorEvents()
{
	if (FSlateApplication::IsInitialized())
	{
		ModalLoopTickHandle = FSlateApplication::Get().GetOnModalLoopTickEvent().AddUObject(this, &UProtoBoySubsystem::OnModalLoopTick);
	}

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

	// TODO: Remove the handles bound in BindEditorEvents.
}

void UProtoBoySubsystem::OnModalLoopTick(float DeltaTime)
{
	MCPServer->TickWhileModal(DeltaTime);
	TrelloConnector->TickWhileModal(DeltaTime);
}
