#pragma once

#include "CoreMinimal.h"

/**
 * Markdown helpers for docs that humans and agents both edit.
 *
 * ProtoBoy never rewrites human text. It either appends to a section, or replaces only the inside of a
 * managed block it owns:
 *     <!-- protoboy:begin some-id -->
 *     ...content ProtoBoy may replace...
 *     <!-- protoboy:end some-id -->
 */
namespace ProtoBoyMarkdown
{
	/** A heading and the text under it, up to the next heading of the same or higher level. */
	struct FSection
	{
		FString Heading;
		int32 Level = 0;
		/** Offset of the heading line. */
		int32 HeadingStart = 0;
		/** Offset just after the heading line (start of the body). */
		int32 BodyStart = 0;
		/** Offset where the section ends (next same-or-higher heading, or end of text). */
		int32 End = 0;
	};

	PROTOBOY_API TArray<FSection> ParseSections(const FString& Text);

	/** First section whose heading matches (case-insensitive, ignoring surrounding whitespace and '#'). */
	PROTOBOY_API const FSection* FindSection(const TArray<FSection>& Sections, const FString& Heading);

	/** The body of a section, trimmed. Empty if not found. */
	PROTOBOY_API FString GetSectionBody(const FString& Text, const FString& Heading);

	/** The document's first "# " heading, or empty. */
	PROTOBOY_API FString GetTitle(const FString& Text);

	/**
	 * Append Content at the end of the section with this heading. If there's no such section, adds "## Heading"
	 * at the end of the document. Existing text is never changed.
	 */
	PROTOBOY_API FString AppendToSection(const FString& Text, const FString& Heading, const FString& Content);

	/**
	 * Replace the inside of managed block BlockId with Content. If the block doesn't exist, it's added at the end of
	 * the section with this heading (or at the end of the document when Heading is empty), creating the section if needed.
	 */
	PROTOBOY_API FString SetManagedBlock(const FString& Text, const FString& Heading, const FString& BlockId, const FString& Content);

	/** True if BlockId is a safe managed-block id: lowercase letters, digits and '-'. */
	PROTOBOY_API bool IsValidBlockId(const FString& BlockId);
}
