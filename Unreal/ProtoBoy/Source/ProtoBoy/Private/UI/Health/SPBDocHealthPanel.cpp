#include "UI/Health/SPBDocHealthPanel.h"

#include "Health/PBDocHealth.h"
#include "PBSettings.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SProtoBoyDocHealthPanel"

void SProtoBoyDocHealthPanel::Construct(const FArguments& InArgs)
{
	auto IsConnected = []() { FText Ignored; return ProtoBoyDocHealth::IsRevisionControlConnected(Ignored); };

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("Intro", "Checking this allows you to run \"Health Checks\" using an LLM. If you are connected to Revision Control in the engine, it will check for files that have been modified that may be related to certain existing notes. That way, you can check to see if any game documentation may need updating."))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.AutoWrapText(true)
		]

		// The setting: only while revision control is connected
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SCheckBox)
			.IsEnabled_Lambda(IsConnected)
			.IsChecked_Lambda([IsConnected]()
			{
				return IsConnected() && GetDefault<UProtoBoySettings>()->bEnableDocHealthChecks ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([](ECheckBoxState State)
			{
				UProtoBoySettings* Settings = GetMutableDefault<UProtoBoySettings>();
				Settings->bEnableDocHealthChecks = State == ECheckBoxState::Checked;
				Settings->TryUpdateDefaultConfigFile();
			})
			.ToolTipText_Lambda([IsConnected]()
			{
				return IsConnected()
					? LOCTEXT("EnableTip", "Shared by the team (saved in DefaultProtoBoy.ini). Health checks don't run yet.")
					: LOCTEXT("EnableDisabledTip", "Connect the editor to revision control first (see below).");
			})
			[
				SNew(STextBlock).Text(LOCTEXT("Enable", "Enable documentation health checks"))
			]
		]

		// Revision control status
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image_Lambda([IsConnected]() { return FAppStyle::Get().GetBrush(IsConnected() ? "Icons.SuccessWithColor" : "Icons.WarningWithColor"); })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([]() { FText Status; ProtoBoyDocHealth::IsRevisionControlConnected(Status); return Status; })
				.AutoWrapText(true)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 0.f, 0.f)
			[
				SNew(SButton)
				.Text_Lambda([IsConnected]() { return IsConnected() ? LOCTEXT("ChangeRC", "Revision Control Settings...") : LOCTEXT("ConnectRC", "Connect to Revision Control..."); })
				.OnClicked_Lambda([]() { ProtoBoyDocHealth::OpenRevisionControlLogin(); return FReply::Handled(); })
			]
		]

		// How to connect, until connected
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 10.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)
			.Visibility_Lambda([IsConnected]() { return IsConnected() ? EVisibility::Collapsed : EVisibility::Visible; })

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HowToHeading", "How to connect Unreal to revision control"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("HowToSteps",
					"1. Click Connect to Revision Control... above (or Revision Control in the editor's bottom-right status bar, then Connect to Revision Control).\n"
					"2. Pick your provider: Perforce, Git, Unity Version Control (Plastic SCM) or Subversion. Git needs the Git plugin enabled under Edit > Plugins.\n"
					"3. Fill in the connection details (server and workspace for Perforce; the repository root and git.exe path for Git) and click Accept Settings.\n"
					"4. Once the status above says Connected, tick Enable documentation health checks."))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.AutoWrapText(true)
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
