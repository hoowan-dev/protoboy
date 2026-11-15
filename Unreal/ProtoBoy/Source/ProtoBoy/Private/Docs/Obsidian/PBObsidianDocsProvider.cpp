#include "Docs/Obsidian/PBObsidianDocsProvider.h"

#include "Docs/Obsidian/PBObsidianConnector.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "ProtoBoyObsidianDocsProvider"

FProtoBoyObsidianDocsProvider::FProtoBoyObsidianDocsProvider(const FProtoBoyObsidianConnector& InConnector)
	: Connector(InConnector)
{
}

FText FProtoBoyObsidianDocsProvider::GetDisplayName() const
{
	return FText::Format(LOCTEXT("Name", "Obsidian (vault \"{0}\")"), FText::FromString(Connector.GetVaultName()));
}

FString FProtoBoyObsidianDocsProvider::GetDocsRoot() const
{
	return Connector.IsConnected() ? Connector.GetDocsRoot() : FString();
}

bool FProtoBoyObsidianDocsProvider::IsReady(FText& OutWhyNot) const
{
	if (!Connector.IsConnected())
	{
		OutWhyNot = LOCTEXT("NotConnected", "Obsidian isn't connected.");
		return false;
	}
	return FProtoBoyFolderDocsProvider::IsReady(OutWhyNot);
}

FString FProtoBoyObsidianDocsProvider::GetTrashDir() const
{
	// Obsidian's own "move to .trash" location, so deleted notes show up where Obsidian users look for them.
	return Connector.IsConnected() ? FPaths::Combine(Connector.GetVaultPath(), TEXT(".trash")) : FProtoBoyFolderDocsProvider::GetTrashDir();
}

FText FProtoBoyObsidianDocsProvider::GetDeletedDocsLocation() const
{
	return Connector.IsConnected() ? LOCTEXT("ObsidianTrash", "the vault's .trash folder") : FProtoBoyFolderDocsProvider::GetDeletedDocsLocation();
}

#undef LOCTEXT_NAMESPACE
