#include "PBModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "ToolMenus.h"
#include "UI/PBStyle.h"
#include "UI/SPBDashboard.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FProtoBoyModule"

DEFINE_LOG_CATEGORY(LogProtoBoy);

const FName FProtoBoyModule::DashboardTabName(TEXT("ProtoBoyDashboard"));

void FProtoBoyModule::StartupModule()
{
	FProtoBoyStyle::Initialize();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DashboardTabName,
		FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
		{
			return SNew(SDockTab)
				.TabRole(ETabRole::NomadTab)
				[
					SNew(SProtoBoyDashboard)
				];
		}))
		.SetDisplayName(LOCTEXT("DashboardTabTitle", "ProtoBoy"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FProtoBoyModule::RegisterMenus));
}

void FProtoBoyModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DashboardTabName);
	}

	FProtoBoyStyle::Shutdown();
}

void FProtoBoyModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("ProtoBoy", LOCTEXT("ProtoBoySection", "ProtoBoy"));
	Section.AddMenuEntry(
		"OpenProtoBoyDashboard",
		LOCTEXT("OpenDashboard", "ProtoBoy Dashboard"),
		LOCTEXT("OpenDashboardTooltip", "Open the ProtoBoy dashboard."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(DashboardTabName);
		})));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FProtoBoyModule, ProtoBoy)
