#include "UI/PBStatusBarHub.h"

#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "UI/PBStyle.h"
#include "UI/SPBDashboard.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyStatusBarHub"

namespace ProtoBoyStatusBarHub
{
	const FName MenuName(TEXT("ProtoBoy.StatusBar.Hub"));
	/** Everything the hub adds to tool menus is owned by this name, so Unregister can remove it. */
	const FName OwnerName(TEXT("ProtoBoyStatusBarHub"));
	/** The level editor's status bar toolbar: added up front, as its bar may exist before Register runs. */
	const FName LevelEditorToolBarName(TEXT("LevelEditor.StatusBar.ToolBar"));
	/** SStatusBar registers every status bar's toolbar menu with this style; nothing else uses it. */
	const FName StatusBarStyleName(TEXT("StatusBarToolBar"));

	/** The status bar toolbar menus that already have the hub. */
	TSet<FName> ToolBarsWithHub;
	FDelegateHandle PreGenerateHandle;

	/** The hub's menu: the dashboard, then one entry per dashboard page. */
	void RegisterMenu()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		// Still registered after a module reload, but emptied by Unregister: fill it again (same names replace).
		UToolMenu* Menu = ToolMenus->IsMenuRegistered(MenuName) ? ToolMenus->ExtendMenu(MenuName) : ToolMenus->RegisterMenu(MenuName);

		FToolMenuSection& DashboardSection = Menu->FindOrAddSection("Dashboard");
		DashboardSection.AddMenuEntry(
			"OpenDashboard",
			LOCTEXT("OpenDashboard", "Open Dashboard"),
			LOCTEXT("OpenDashboardTip", "Open the ProtoBoy dashboard on the page it was last on."),
			FSlateIcon(FProtoBoyStyle::Get().GetStyleSetName(), "ProtoBoy.Logo"),
			FUIAction(FExecuteAction::CreateLambda([]() { SProtoBoyDashboard::Open(); })));

		FToolMenuSection& PagesSection = Menu->FindOrAddSection("Pages", LOCTEXT("PagesSection", "Pages"));
		const TArray<SProtoBoyDashboard::FPage> Pages = SProtoBoyDashboard::GetPageDefinitions();
		for (int32 Index = 0; Index < Pages.Num(); ++Index)
		{
			PagesSection.AddMenuEntry(
				FName(*FString::Printf(TEXT("Page%d"), Index)),
				Pages[Index].Label,
				Pages[Index].Description,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), Pages[Index].Icon),
				FUIAction(FExecuteAction::CreateLambda([Index]() { SProtoBoyDashboard::Open(Index); })));
		}
	}

	/** The button: logo and name, styled like the Revision Control button beside it. */
	TSharedRef<SWidget> MakeButton()
	{
		return SNew(SComboButton)
			.ContentPadding(FMargin(6.f, 0.f))
			.ToolTipText(LOCTEXT("HubTip", "ProtoBoy: open the dashboard, or go straight to one of its pages."))
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
			]
			.OnGetMenuContent(FOnGetContent::CreateLambda([]() { return UToolMenus::Get()->GenerateWidget(MenuName, FToolMenuContext()); }));
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
			RegisterMenu();
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
