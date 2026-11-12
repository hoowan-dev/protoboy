#include "Docs/Obsidian/PBObsidianDocsProvider.h"

#include "Docs/Obsidian/PBObsidianConnector.h"

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

#undef LOCTEXT_NAMESPACE
