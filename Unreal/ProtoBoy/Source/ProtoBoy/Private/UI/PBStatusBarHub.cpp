#include "UI/PBStatusBarHub.h"

#include "Docs/PBDocMap.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Health/PBDocHealth.h"
#include "Health/PBDocHealthService.h"
#include "PBSubsystem.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "ToolMenus.h"
#include "UI/PBStyle.h"
#include "UI/SPBDashboard.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyStatusBarHub"

namespace ProtoBoyStatusBarHub
{
	const FName MenuName(TEXT("ProtoBoy.StatusBar.Hub"));
	const FName HealthMenuName(TEXT("ProtoBoy.StatusBar.Hub.Health"));
	/** Everything the hub adds to tool menus is owned by this name, so Unregister can remove it. */
	const FName OwnerName(TEXT("ProtoBoyStatusBarHub"));
	/** The level editor's status bar toolbar: added up front, as its bar may exist before Register runs. */
	const FName LevelEditorToolBarName(TEXT("LevelEditor.StatusBar.ToolBar"));
	/** SStatusBar registers every status bar's toolbar menu with this style; nothing else uses it. */
	const FName StatusBarStyleName(TEXT("StatusBarToolBar"));
	/** Docs listed in the menu; the rest are on the Health page. */
	constexpr int32 MaxListedDocs = 8;

	/** The status bar toolbar menus that already have the hub. */
	TSet<FName> ToolBarsWithHub;
	FDelegateHandle PreGenerateHandle;

	/** Null before the editor's subsystems exist and while it shuts down. */
	FProtoBoyDocHealthService* GetHealth()
	{
		UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		return Subsystem ? &Subsystem->GetDocHealth() : nullptr;
	}

	/** A toast: there's no room for messages in the status bar itself. */
	void Notify(const FText& Message, bool bError)
	{
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 6.f;
		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid() && bError)
		{
			Item->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}

	// --- Health status ------------------------------------------------------------------------------------------------

	/** Docs the last report says may need updating. */
	int32 CountStaleDocs(const FProtoBoyDocHealthService& Health)
	{
		return Health.HasReport() ? Health.GetReport().Docs.Num() : 0;
	}

	/** Something on the Health page wants attention. */
	bool NeedsAttention()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		return Health && !Health->IsChecking() && (CountStaleDocs(*Health) > 0 || Health->CountProblems() > 0);
	}

	bool IsChecking()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		return Health && Health->IsChecking();
	}

	/** The icon beside the button's name: a warning when something wants attention, a check when healthy, a question mark when unchecked. */
	const FSlateBrush* GetStatusIcon()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		const bool bChecked = Health && (Health->HasReport() || Health->IsStructureChecked());
		return FAppStyle::Get().GetBrush(NeedsAttention() ? "Icons.WarningWithColor" : bChecked ? "Icons.SuccessWithColor" : "Icons.Help");
	}

	/** Where the docs stand, in a few words: the tooltip's first line. */
	FText GetHeadline()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		if (!Health)
		{
			return LOCTEXT("LabelUnavailable", "Not available yet");
		}
		if (Health->IsChecking())
		{
			return LOCTEXT("LabelChecking", "Checking docs...");
		}
		const int32 NumStale = CountStaleDocs(*Health);
		const int32 NumProblems = Health->CountProblems();
		if (NumStale > 0 && NumProblems > 0)
		{
			return FText::Format(LOCTEXT("LabelBoth", "{0} {0}|plural(one=doc,other=docs) to update, {1} {1}|plural(one=problem,other=problems)"), NumStale, NumProblems);
		}
		if (NumStale > 0)
		{
			return FText::Format(LOCTEXT("LabelStale", "{0} {0}|plural(one=doc,other=docs) to update"), NumStale);
		}
		if (NumProblems > 0)
		{
			return FText::Format(LOCTEXT("LabelProblems", "{0} doc {0}|plural(one=problem,other=problems)"), NumProblems);
		}
		return Health->HasReport() || Health->IsStructureChecked() ? LOCTEXT("LabelHealthy", "Docs look healthy") : LOCTEXT("LabelUnchecked", "Docs not checked");
	}

	/** The detail behind the label: the last check against revision control, then the structure checks. */
	FText GetStatusText()
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		if (!Health)
		{
			return FText::GetEmpty();
		}

		FText Report = Health->DescribeReport();
		if (Health->IsChecking())
		{
			Report = LOCTEXT("StatusChecking", "Checking the docs against revision control. This can take a few minutes.");
		}
		else if (Report.IsEmpty())
		{
			Report = ProtoBoyDocHealth::IsEnabled()
				? LOCTEXT("StatusNoReport", "The docs haven't been checked against revision control yet.")
				: LOCTEXT("StatusDisabled", "Checking the docs against revision control is off (Setup > Documentation Health Checks).");
		}

		FText Structure = LOCTEXT("StatusNoStructure", "Structure: not checked this session.");
		if (Health->IsStructureChecked())
		{
			const int32 NumProblems = Health->CountProblems();
			const int32 NumNotes = Health->GetIssues().Num() - NumProblems;
			Structure = Health->GetIssues().IsEmpty()
				? LOCTEXT("StatusStructureClean", "Structure: nothing found.")
				: FText::Format(LOCTEXT("StatusStructure", "Structure: {0} {0}|plural(one=problem,other=problems) and {1} {1}|plural(one=thing,other=things) to fill in."), NumProblems, NumNotes);
		}
		return FText::Format(LOCTEXT("StatusFormat", "{0}\n{1}"), Report, Structure);
	}

	// --- Actions ------------------------------------------------------------------------------------------------------

	/** Run everything that can run: the structure checks, then the check against revision control. Cancels a running check instead. */
	void RunOrCancelChecks()
	{
		FProtoBoyDocHealthService* Health = GetHealth();
		if (!Health)
		{
			return;
		}
		if (Health->IsChecking())
		{
			Health->CancelCheck();
			return;
		}

		Health->CheckStructure();
		if (!ProtoBoyDocHealth::IsEnabled())
		{
			Notify(LOCTEXT("OnlyStructure", "Checked the docs' structure. Checking them against revision control is off: turn it on in Setup > Documentation Health Checks."), false);
			return;
		}
		FText Error;
		if (!Health->StartCheck(Error))
		{
			Notify(Error, true);
		}
	}

	void OpenStaleDoc(const FString& DocId, const FString& Path, const FString& Title)
	{
		const UProtoBoySubsystem* Subsystem = UProtoBoySubsystem::Get();
		if (!Subsystem)
		{
			return;
		}
		// A saved report may be older than the map: the doc may have moved or gone since.
		const FProtoBoyDocNode* Doc = Subsystem->GetDocMap().Find(DocId);
		if (!Doc)
		{
			Doc = Subsystem->GetDocMap().FindByPath(Path);
		}
		FText Error;
		if (!Doc)
		{
			Notify(FText::Format(LOCTEXT("DocGone", "\"{0}\" isn't on the doc map any more."), FText::FromString(Title)), true);
		}
		else if (!Subsystem->GetDocsService().OpenDoc(Doc->Path, Error))
		{
			Notify(Error, true);
		}
	}

	// --- Menu ---------------------------------------------------------------------------------------------------------

	/** The docs the last report flagged, most confident first; the menu is rebuilt each time it opens. */
	void AddStaleDocs(FToolMenuSection& Section)
	{
		const FProtoBoyDocHealthService* Health = GetHealth();
		if (!Health || !Health->HasReport())
		{
			return;
		}
		const TArray<FProtoBoyStaleDoc>& Docs = Health->GetReport().Docs;
		for (int32 Index = 0; Index < Docs.Num() && Index < MaxListedDocs; ++Index)
		{
			const FProtoBoyStaleDoc& Doc = Docs[Index];
			const FString DocId = Doc.DocId;
			const FString Path = Doc.Path;
			const FString Title = Doc.Title;
			Section.AddMenuEntry(
				FName(*FString::Printf(TEXT("StaleDoc%d"), Index)),
				FText::FromString(Doc.Title),
				FText::Format(LOCTEXT("StaleDocTip", "{0}\n\nClick to open {1}."), FText::FromString(Doc.Reason), FText::FromString(Doc.Path)),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.WarningWithColor"),
				FUIAction(FExecuteAction::CreateLambda([DocId, Path, Title]() { OpenStaleDoc(DocId, Path, Title); })));
		}
		if (Docs.Num() > MaxListedDocs)
		{
			Section.AddMenuEntry(
				"MoreStaleDocs",
				FText::Format(LOCTEXT("MoreStaleDocs", "...and {0} more on the Health page"), Docs.Num() - MaxListedDocs),
				LOCTEXT("MoreStaleDocsTip", "Open the Health page to see every doc that may need updating."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]() { SProtoBoyDashboard::Open(SProtoBoyDashboard::HealthPageIndex); })));
		}
	}

	/** Register a menu, or get it again after a module reload: still registered then, but emptied by Unregister (same names replace). */
	UToolMenu* FindOrRegisterMenu(const FName Name)
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		return ToolMenus->IsMenuRegistered(Name) ? ToolMenus->ExtendMenu(Name) : ToolMenus->RegisterMenu(Name);
	}

	/** The hub's two menus, shown side by side (MakeMenuContent): the dashboard's pages, and the health status and checks. */
	void RegisterMenus()
	{
		UToolMenu* HealthMenu = FindOrRegisterMenu(HealthMenuName);
		FToolMenuSection& HealthSection = HealthMenu->FindOrAddSection("Health", LOCTEXT("HealthSection", "Documentation Health"));
		// Dynamic, so each opened menu gets its own text widget.
		HealthSection.AddDynamicEntry("Status", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			InSection.AddEntry(FToolMenuEntry::InitWidget("Status",
				SNew(SBox)
				.WidthOverride(360.f)
				.Padding(FMargin(12.f, 2.f, 12.f, 6.f))
				[
					SNew(STextBlock)
					.Text_Lambda([]() { return GetStatusText(); })
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				],
				FText::GetEmpty(), /*bNoIndent*/ true, /*bSearchable*/ false));
		}));
		HealthSection.AddMenuEntry(
			"RunChecks",
			TAttribute<FText>::CreateLambda([]() { return IsChecking() ? LOCTEXT("CancelChecks", "Cancel Health Checks") : LOCTEXT("RunChecks", "Run Health Checks"); }),
			TAttribute<FText>::CreateLambda([]()
			{
				return IsChecking()
					? LOCTEXT("CancelChecksTip", "Stop the agent. The last report stays.")
					: LOCTEXT("RunChecksTip", "Check the docs' structure, then have your agent find the docs that changes in revision control have outdated. That part runs in the background and can take a few minutes; this button shows the result.");
			}),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"),
			FUIAction(FExecuteAction::CreateStatic(&RunOrCancelChecks)));
		HealthSection.AddDynamicEntry("StaleDocs", FNewToolMenuSectionDelegate::CreateStatic(&AddStaleDocs));

		UToolMenu* PagesMenu = FindOrRegisterMenu(MenuName);
		FToolMenuSection& DashboardSection = PagesMenu->FindOrAddSection("Dashboard", LOCTEXT("DashboardSection", "Dashboard"));
		DashboardSection.AddMenuEntry(
			"OpenDashboard",
			LOCTEXT("OpenDashboard", "Open Dashboard"),
			LOCTEXT("OpenDashboardTip", "Open the ProtoBoy dashboard on the page it was last on."),
			FSlateIcon(FProtoBoyStyle::Get().GetStyleSetName(), "ProtoBoy.Logo"),
			FUIAction(FExecuteAction::CreateLambda([]() { SProtoBoyDashboard::Open(); })));
		const TArray<SProtoBoyDashboard::FPage> Pages = SProtoBoyDashboard::GetPageDefinitions();
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			DashboardSection.AddMenuEntry(
				FName(*FString::Printf(TEXT("Page%d"), Index)),
				Pages[Index].Label,
				Pages[Index].Description,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), Pages[Index].Icon),
				FUIAction(FExecuteAction::CreateLambda([Index]() { SProtoBoyDashboard::Open(Index); })));
		}
	}

	/** What the button opens: the page options on the left, the health status and checks to their right. Built each time it opens. */
	TSharedRef<SWidget> MakeMenuContent()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			[
				ToolMenus->GenerateWidget(MenuName, FToolMenuContext())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
				.Thickness(1.f)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			[
				ToolMenus->GenerateWidget(HealthMenuName, FToolMenuContext())
			];
	}

	// --- Button -------------------------------------------------------------------------------------------------------

	/**
	 * The button: the logo and "ProtoBoy", then the health status as an icon (a throbber while checking), styled like the
	 * Revision Control button beside it. What the icon means is spelled out in the tooltip.
	 */
	TSharedRef<SWidget> MakeButton()
	{
		return SNew(SComboButton)
			.ContentPadding(FMargin(6.f, 0.f))
			.ToolTipText_Lambda([]()
			{
				return FText::Format(LOCTEXT("HubTip", "Documentation health: {0}\n\n{1}\n\nClick to run the checks, open a doc that needs updating, or open the dashboard."), GetHeadline(), GetStatusText());
			})
			// The status bar is at the bottom of the window.
			.MenuPlacement(MenuPlacement_AboveAnchor)
			.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
			.ButtonContent()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(FProtoBoyStyle::Get().GetBrush("ProtoBoy.Logo"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(5.f, 0.f, 0.f, 0.f)
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
					.Text(LOCTEXT("HubLabel", "ProtoBoy"))
				]

				// Health status: the icon, or a throbber while a check runs.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SImage)
					.Visibility_Lambda([]() { return IsChecking() ? EVisibility::Collapsed : EVisibility::Visible; })
					.Image_Lambda([]() { return GetStatusIcon(); })
					// The warning and success icons carry their own color; the question mark is plain.
					.ColorAndOpacity_Lambda([]() { return GetStatusIcon() == FAppStyle::Get().GetBrush("Icons.Help") ? FSlateColor::UseSubduedForeground() : FSlateColor(FLinearColor::White); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					SNew(SCircularThrobber)
					.Radius(7.f)
					.Visibility_Lambda([]() { return IsChecking() ? EVisibility::Visible : EVisibility::Collapsed; })
				]
			]
			.OnGetMenuContent(FOnGetContent::CreateStatic(&MakeMenuContent));
	}

	/** Add the hub to one status bar toolbar menu, once. Works before the menu is registered too. */
	void AddToToolBar(const FName ToolBarName)
	{
		if (ToolBarsWithHub.Contains(ToolBarName))
		{
			return;
		}
		ToolBarsWithHub.Add(ToolBarName);

		FToolMenuOwnerScoped OwnerScoped(OwnerName);
		UToolMenu* ToolBar = UToolMenus::Get()->ExtendMenu(ToolBarName);
		// Bars that hide Revision Control have no such section; the hub then goes at the end.
		FToolMenuSection& Section = ToolBar->AddSection("ProtoBoy", FText::GetEmpty(), FToolMenuInsert("SourceControl", EToolMenuInsertType::Before));
		// Dynamic, so each status bar that shares this toolbar menu gets a button of its own (a widget has one parent).
		Section.AddDynamicEntry("ProtoBoyHub", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			InSection.AddEntry(FToolMenuEntry::InitWidget("ProtoBoyHub", MakeButton(), FText::GetEmpty(), /*bNoIndent*/ true, /*bSearchable*/ false));
		}));
	}

	/** Before any tool menu widget is generated: a status bar toolbar gets the hub before its first widget is built. */
	void OnPreGenerateWidget(const FName GeneratedMenuName, const FToolMenuContext& Context)
	{
		if (ToolBarsWithHub.Contains(GeneratedMenuName))
		{
			return;
		}
		const UToolMenu* Menu = UToolMenus::Get()->FindMenu(GeneratedMenuName);
		if (Menu && Menu->StyleName == StatusBarStyleName)
		{
			AddToToolBar(GeneratedMenuName);
		}
	}

	void Register()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		{
			FToolMenuOwnerScoped OwnerScoped(OwnerName);
			RegisterMenus();
		}
		AddToToolBar(LevelEditorToolBarName);
		if (!PreGenerateHandle.IsValid())
		{
			PreGenerateHandle = ToolMenus->OnPreGenerateWidget.AddStatic(&OnPreGenerateWidget);
		}
		// The level editor's bar may have been built already: rebuild it with the hub (next tick).
		ToolMenus->RefreshAllWidgets();
	}

	void Unregister()
	{
		if (UToolMenus* ToolMenus = UToolMenus::TryGet())
		{
			ToolMenus->OnPreGenerateWidget.Remove(PreGenerateHandle);
			ToolMenus->UnregisterOwnerByName(OwnerName);
		}
		PreGenerateHandle.Reset();
		ToolBarsWithHub.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
