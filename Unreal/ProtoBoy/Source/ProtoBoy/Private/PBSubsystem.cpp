#include "PBSubsystem.h"

#include "Editor.h"
#include "PBModule.h"
#include "PBSettings.h"

void UProtoBoySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DocGenerator = MakeUnique<FProtoBoyDocGenerator>();
	HealthChecker = MakeUnique<FProtoBoyHealthChecker>();
	AssetLifecycle = MakeUnique<FProtoBoyAssetLifecycle>();
	DaemonClient = MakeUnique<FProtoBoyDaemonClient>();

	const UProtoBoySettings* Settings = GetDefault<UProtoBoySettings>();
	if (Settings->bConnectToDaemon)
	{
		DaemonClient->Connect(Settings->DaemonUrl);
	}

	BindEditorEvents();
	UE_LOG(LogProtoBoy, Log, TEXT("ProtoBoy subsystem initialized."));
}

void UProtoBoySubsystem::Deinitialize()
{
	UnbindEditorEvents();
	DaemonClient->Disconnect();

	DaemonClient.Reset();
	AssetLifecycle.Reset();
	HealthChecker.Reset();
	DocGenerator.Reset();

	Super::Deinitialize();
}

UProtoBoySubsystem* UProtoBoySubsystem::Get()
{
	return GEditor ? GEditor->GetEditorSubsystem<UProtoBoySubsystem>() : nullptr;
}

void UProtoBoySubsystem::BindEditorEvents()
{
	// TODO: IAssetRegistry::OnAssetAdded/Removed/Renamed, UPackage::PackageSavedWithContextEvent,
	//       ILiveCodingModule::GetOnPatchCompleteDelegate.
}

void UProtoBoySubsystem::UnbindEditorEvents()
{
	// TODO: Remove the handles bound in BindEditorEvents.
}
