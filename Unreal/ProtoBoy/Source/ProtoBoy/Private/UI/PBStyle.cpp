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

	// ProtoBoy's logo (the dashboard tab and the Tools menu entry). White like the editor's own icons, so Slate tints it.
	const FString Logo = StyleSet->RootToContentDir(TEXT("Icons/ProtoBoy"), TEXT(".svg"));
	StyleSet->Set("ProtoBoy.Logo", new FSlateVectorImageBrush(Logo, FVector2D(16.f, 16.f)));
	// The dashboard sidebar's title.
	StyleSet->Set("ProtoBoy.Logo.Large", new FSlateVectorImageBrush(Logo, FVector2D(36.f, 36.f)));

	const FString ObsidianIcon = StyleSet->RootToContentDir(TEXT("Icons/Obsidian"), TEXT(".svg"));
	StyleSet->Set("ProtoBoy.Icon.Obsidian", new FSlateVectorImageBrush(ObsidianIcon, FVector2D(20.f, 20.f)));
	StyleSet->Set("ProtoBoy.Icon.Obsidian.Large", new FSlateVectorImageBrush(ObsidianIcon, FVector2D(40.f, 40.f)));

	// Provider logos: LLM providers (Connect LLM Provider), task boards (Connect Task Board), asset sources.
	for (const TCHAR* Provider : { TEXT("Claude"), TEXT("Gemini"), TEXT("Trello"), TEXT("Fab") })
	{
		const FString Icon = StyleSet->RootToContentDir(FString::Printf(TEXT("Icons/%s"), Provider), TEXT(".svg"));
		StyleSet->Set(*FString::Printf(TEXT("ProtoBoy.Icon.%s"), Provider), new FSlateVectorImageBrush(Icon, FVector2D(20.f, 20.f)));
		StyleSet->Set(*FString::Printf(TEXT("ProtoBoy.Icon.%s.Large"), Provider), new FSlateVectorImageBrush(Icon, FVector2D(40.f, 40.f)));
	}

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
