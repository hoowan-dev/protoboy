#include "UI/Agents/SPBAgentPicker.h"

#include "Agents/PBAgentRun.h"
#include "MCP/PBMCPClientConfig.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "UI/PBStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ProtoBoyAgentPicker"

TOptional<EProtoBoyMCPClient> SProtoBoyAgentPicker::GetDefaultAgent()
{
	for (const EProtoBoyMCPClient Agent : FProtoBoyAgentRun::GetSetUpAgents())
	{
		FText Ignored;
		if (FProtoBoyAgentRun::CanRun(Agent, Ignored))
		{
			return Agent;
		}
	}
	return TOptional<EProtoBoyMCPClient>();
}

void SProtoBoyAgentPicker::Construct(const FArguments& InArgs)
{
	SelectedAgent = InArgs._SelectedAgent;
	OnAgentPicked = InArgs._OnAgentPicked;

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const TArray<EProtoBoyMCPClient> Agents = FProtoBoyAgentRun::GetSetUpAgents();
	for (const EProtoBoyMCPClient Agent : Agents)
	{
		List->AddSlot().AutoHeight().Padding(0.f, 2.f)[ MakeAgentRow(Agent) ];
	}
	if (Agents.IsEmpty())
	{
		List->AddSlot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoAgents", "No agents are set up yet. Connect Claude Code or Gemini CLI in MCP Settings (Setup > MCP on the dashboard), then come back."))
			.ColorAndOpacity(FStyleColors::Warning)
			.AutoWrapText(true)
		];
	}
	ChildSlot[ List ];
}

TSharedRef<SWidget> SProtoBoyAgentPicker::MakeAgentRow(EProtoBoyMCPClient Agent)
{
	FText WhyNot;
	const bool bCanRun = FProtoBoyAgentRun::CanRun(Agent, WhyNot);
	const EProtoBoyLLMProvider Provider = Agent == EProtoBoyMCPClient::GeminiCLI ? EProtoBoyLLMProvider::Gemini : EProtoBoyLLMProvider::Claude;

	return SNew(SCheckBox)
		.Style(FAppStyle::Get(), "RadioButton")
		.IsEnabled(bCanRun)
		.IsChecked_Lambda([this, Agent]()
		{
			const TOptional<EProtoBoyMCPClient> Selected = SelectedAgent.Get();
			return Selected.IsSet() && Selected.GetValue() == Agent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Agent](ECheckBoxState) { OnAgentPicked.ExecuteIfBound(Agent); })
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 6.f, 0.f)
			[
				SNew(SImage)
				.Image(FProtoBoyStyle::Get().GetBrush(FProtoBoyMCPClientConfig::GetProviderIcon(Provider)))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock).Text(FProtoBoyMCPClientConfig::GetClientName(Agent))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(bCanRun ? EVisibility::Collapsed : EVisibility::Visible)
					.Text(WhyNot)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.AutoWrapText(true)
				]
			]
		];
}

#undef LOCTEXT_NAMESPACE
