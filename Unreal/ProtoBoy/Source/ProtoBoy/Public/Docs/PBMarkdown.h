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

	/** Text with every HTML comment (hints, managed-block markers) removed and trimmed: what a reader actually wrote. */
	PROTOBOY_API FString StripComments(const FString& Text);

	/**
	 * YAML frontmatter: a block at the very top between two "---" lines (what Obsidian shows as properties).
	 * Only flat "key: value" lines are read; lists and nested values are kept as they are but not parsed.
	 */
	struct FFrontmatter
	{
		bool bPresent = false;
		TArray<TPair<FString, FString>> Values;
		/** Offset just after the closing "---" line (0 without frontmatter). Headings are only looked for after it. */
		int32 BodyStart = 0;

		/** Value of Key (case-insensitive), or empty. */
		PROTOBOY_API FString Get(const FString& Key) const;
	};

	PROTOBOY_API FFrontmatter ParseFrontmatter(const FString& Text);

	/**
	 * Set keys in the frontmatter, creating it at the top if missing. An empty value removes the key. Every other line
	 * (the team's own properties) is kept as it is.
	 */
	PROTOBOY_API FString SetFrontmatterValues(const FString& Text, const TArray<TPair<FString, FString>>& Values);
}
