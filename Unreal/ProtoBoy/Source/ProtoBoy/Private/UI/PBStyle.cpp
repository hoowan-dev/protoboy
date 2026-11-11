#include "UI/PBStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

TSharedPtr<FSlateStyleSet> FProtoBoyStyle::StyleSet;

void FProtoBoyStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(TEXT("ProtoBoyStyle"));

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ProtoBoy"));
	check(Plugin.IsValid());
	StyleSet->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));

	const FString ObsidianIcon = StyleSet->RootToContentDir(TEXT("Icons/Obsidian"), TEXT(".svg"));
	StyleSet->Set("ProtoBoy.Icon.Obsidian", new FSlateVectorImageBrush(ObsidianIcon, FVector2D(20.f, 20.f)));
	StyleSet->Set("ProtoBoy.Icon.Obsidian.Large", new FSlateVectorImageBrush(ObsidianIcon, FVector2D(40.f, 40.f)));

	// LLM provider logos (Connect LLM Provider dialog).
	StyleSet->Set("ProtoBoy.Icon.Claude",
		new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("Icons/Claude"), TEXT(".svg")), FVector2D(20.f, 20.f)));
	StyleSet->Set("ProtoBoy.Icon.Gemini",
		new FSlateVectorImageBrush(StyleSet->RootToContentDir(TEXT("Icons/Gemini"), TEXT(".svg")), FVector2D(20.f, 20.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
}

void FProtoBoyStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
		StyleSet.Reset();
	}
}

const ISlateStyle& FProtoBoyStyle::Get()
{
	check(StyleSet.IsValid());
	return *StyleSet;
}
