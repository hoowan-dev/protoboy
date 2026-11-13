#include "Docs/PBMarkdown.h"

namespace ProtoBoyMarkdown
{
	namespace Detail
	{
		FString NormalizeHeading(const FString& Heading)
		{
			FString Out = Heading.TrimStartAndEnd();
			while (Out.StartsWith(TEXT("#")))
			{
				Out.RightChopInline(1);
			}
			return Out.TrimStartAndEnd();
		}

		/** Returns the heading level (1-6) of a line, or 0 if it isn't an ATX heading. */
		int32 HeadingLevel(const FString& Line, FString& OutHeading)
		{
			int32 Level = 0;
			while (Level < Line.Len() && Line[Level] == TEXT('#'))
			{
				++Level;
			}
			if (Level == 0 || Level > 6 || Level >= Line.Len() || Line[Level] != TEXT(' '))
			{
				return 0;
			}
			OutHeading = Line.Mid(Level + 1).TrimStartAndEnd();
			return Level;
		}

		/**
		 * Offset of the newline ending the line that starts at From, or Text.Len() for the last line. Use this instead of
		 * Find(TEXT("\n"), ..., From) in line loops: FString::Find clamps From to the last character, so searching from
		 * Text.Len() finds a trailing newline again and the loop never ends.
		 */
		int32 FindLineEnd(const FString& Text, int32 From)
		{
			if (From >= Text.Len())
			{
				return Text.Len();
			}
			const int32 At = Text.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			return At == INDEX_NONE ? Text.Len() : At;
		}

		FString BeginMarker(const FString& Id) { return FString::Printf(TEXT("<!-- protoboy:begin %s -->"), *Id); }
		FString EndMarker(const FString& Id) { return FString::Printf(TEXT("<!-- protoboy:end %s -->"), *Id); }

		/** Text with exactly one trailing newline (or empty). */
		FString WithTrailingNewline(const FString& Text)
		{
			FString Out = Text;
			while (Out.EndsWith(TEXT("\n")) || Out.EndsWith(TEXT("\r")))
			{
				Out.LeftChopInline(1);
			}
			return Out.IsEmpty() ? Out : Out + TEXT("\n");
		}
	}

	TArray<FSection> ParseSections(const FString& Text)
	{
		TArray<FSection> Sections;
		bool bInFence = false;

		// Frontmatter isn't Markdown: a YAML comment there would look like a heading.
		int32 LineStart = ParseFrontmatter(Text).BodyStart;
		while (LineStart <= Text.Len())
		{
			const int32 LineEnd = Detail::FindLineEnd(Text, LineStart);
			const FString Line = Text.Mid(LineStart, LineEnd - LineStart).TrimEnd();

			// Headings inside fenced code blocks don't count.
			if (Line.TrimStart().StartsWith(TEXT("```")))
			{
				bInFence = !bInFence;
			}
			else if (!bInFence)
			{
				FString Heading;
				if (const int32 Level = Detail::HeadingLevel(Line, Heading))
				{
					FSection Section;
					Section.Heading = Heading;
					Section.Level = Level;
					Section.HeadingStart = LineStart;
					Section.BodyStart = FMath::Min(LineEnd + 1, Text.Len());
					Sections.Add(Section);
				}
			}

			if (LineEnd >= Text.Len())
			{
				break;
			}
			LineStart = LineEnd + 1;
		}

		// Each section ends at the next heading of the same or higher level.
		for (int32 i = 0; i < Sections.Num(); ++i)
		{
			Sections[i].End = Text.Len();
			for (int32 j = i + 1; j < Sections.Num(); ++j)
			{
				if (Sections[j].Level <= Sections[i].Level)
				{
					Sections[i].End = Sections[j].HeadingStart;
					break;
				}
			}
		}
		return Sections;
	}

	const FSection* FindSection(const TArray<FSection>& Sections, const FString& Heading)
	{
		const FString Wanted = Detail::NormalizeHeading(Heading);
		return Sections.FindByPredicate([&Wanted](const FSection& S) { return S.Heading.Equals(Wanted, ESearchCase::IgnoreCase); });
	}

	FString GetSectionBody(const FString& Text, const FString& Heading)
	{
		const TArray<FSection> Sections = ParseSections(Text);
		const FSection* Section = FindSection(Sections, Heading);
		return Section ? Text.Mid(Section->BodyStart, Section->End - Section->BodyStart).TrimStartAndEnd() : FString();
	}

	FString GetTitle(const FString& Text)
	{
		for (const FSection& Section : ParseSections(Text))
		{
			if (Section.Level == 1)
			{
				return Section.Heading;
			}
		}
		return FString();
	}

	FString AppendToSection(const FString& Text, const FString& Heading, const FString& Content)
	{
		const FString Addition = Detail::WithTrailingNewline(Content);
		const TArray<FSection> Sections = ParseSections(Text);

		if (const FSection* Section = FindSection(Sections, Heading))
		{
			// Insert before the trailing blank lines of the section, then keep one blank line before the next heading.
			FString Before = Text.Left(Section->End);
			const FString After = Text.Mid(Section->End);
			while (Before.EndsWith(TEXT("\n\n")))
			{
				Before.LeftChopInline(1);
			}
			if (!Before.EndsWith(TEXT("\n")))
			{
				Before += TEXT("\n");
			}
			return Before + TEXT("\n") + Addition + (After.IsEmpty() ? FString() : FString(TEXT("\n"))) + After;
		}

		return Detail::WithTrailingNewline(Text) + TEXT("\n## ") + Detail::NormalizeHeading(Heading) + TEXT("\n\n") + Addition;
	}

	FString SetManagedBlock(const FString& Text, const FString& Heading, const FString& BlockId, const FString& Content)
	{
		const FString Begin = Detail::BeginMarker(BlockId);
		const FString End = Detail::EndMarker(BlockId);
		const FString Block = Begin + TEXT("\n") + Detail::WithTrailingNewline(Content) + End;

		const int32 BeginAt = Text.Find(Begin, ESearchCase::CaseSensitive);
		if (BeginAt != INDEX_NONE)
		{
			const int32 EndAt = Text.Find(End, ESearchCase::CaseSensitive, ESearchDir::FromStart, BeginAt);
			if (EndAt != INDEX_NONE)
			{
				return Text.Left(BeginAt) + Block + Text.Mid(EndAt + End.Len());
			}
		}

		if (Heading.IsEmpty())
		{
			return Detail::WithTrailingNewline(Text) + (Text.IsEmpty() ? FString() : FString(TEXT("\n"))) + Block + TEXT("\n");
		}
		return AppendToSection(Text, Heading, Block);
	}

	bool IsValidBlockId(const FString& BlockId)
	{
		if (BlockId.IsEmpty() || BlockId.Len() > 64)
		{
			return false;
		}
		for (const TCHAR C : BlockId)
		{
			if (!(FChar::IsLower(C) || FChar::IsDigit(C) || C == TEXT('-')))
			{
				return false;
			}
		}
		return true;
	}

	FString StripComments(const FString& Text)
	{
		FString Out;
		int32 From = 0;
		while (From < Text.Len())
		{
			const int32 Open = Text.Find(TEXT("<!--"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
			if (Open == INDEX_NONE)
			{
				Out += Text.Mid(From);
				break;
			}
			Out += Text.Mid(From, Open - From);
			const int32 Close = Text.Find(TEXT("-->"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 4);
			if (Close == INDEX_NONE)
			{
				break;
			}
			From = Close + 3;
		}
		return Out.TrimStartAndEnd();
	}

	namespace Detail
	{
		/** The frontmatter's lines (between the fences) and where the block ends. False without frontmatter. */
		bool SplitFrontmatter(const FString& Text, TArray<FString>& OutLines, int32& OutBodyStart)
		{
			if (!Text.StartsWith(TEXT("---")))
			{
				return false;
			}
			const int32 FirstEnd = FindLineEnd(Text, 0);
			if (FirstEnd >= Text.Len() || !Text.Left(FirstEnd).TrimEnd().Equals(TEXT("---")))
			{
				return false;
			}
			TArray<FString> Lines;
			for (int32 LineStart = FirstEnd + 1; LineStart < Text.Len(); )
			{
				const int32 LineEnd = FindLineEnd(Text, LineStart);
				const FString Line = Text.Mid(LineStart, LineEnd - LineStart).TrimEnd();
				if (Line == TEXT("---") || Line == TEXT("..."))
				{
					OutLines = MoveTemp(Lines);
					OutBodyStart = FMath::Min(LineEnd + 1, Text.Len());
					return true;
				}
				Lines.Add(Line);
				LineStart = LineEnd + 1;
			}
			return false;
		}

		/** "key: value" with the key at the start of the line. */
		bool SplitKeyValue(const FString& Line, FString& OutKey, FString& OutValue)
		{
			if (Line.IsEmpty() || FChar::IsWhitespace(Line[0]) || Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("-")))
			{
				return false;
			}
			int32 Colon = INDEX_NONE;
			if (!Line.FindChar(TEXT(':'), Colon) || Colon == 0)
			{
				return false;
			}
			OutKey = Line.Left(Colon).TrimStartAndEnd();
			OutValue = Line.Mid(Colon + 1).TrimStartAndEnd();
			if (OutValue.Len() >= 2 && ((OutValue.StartsWith(TEXT("\"")) && OutValue.EndsWith(TEXT("\""))) || (OutValue.StartsWith(TEXT("'")) && OutValue.EndsWith(TEXT("'")))))
			{
				OutValue = OutValue.Mid(1, OutValue.Len() - 2);
			}
			return true;
		}

		/** Quote a value if YAML would read it as something other than a plain string. */
		FString QuoteValue(const FString& Value)
		{
			const bool bPlain = !Value.Contains(TEXT(":")) && !Value.Contains(TEXT("#")) && !Value.StartsWith(TEXT("["))
				&& !Value.StartsWith(TEXT("{")) && !Value.StartsWith(TEXT("\"")) && !Value.StartsWith(TEXT("'"));
			return bPlain ? Value : FString::Printf(TEXT("\"%s\""), *Value.Replace(TEXT("\""), TEXT("\\\"")));
		}
	}

	FString FFrontmatter::Get(const FString& Key) const
	{
		for (const TPair<FString, FString>& Pair : Values)
		{
			if (Pair.Key.Equals(Key, ESearchCase::IgnoreCase))
			{
				return Pair.Value;
			}
		}
		return FString();
	}

	FFrontmatter ParseFrontmatter(const FString& Text)
	{
		FFrontmatter Out;
		TArray<FString> Lines;
		if (!Detail::SplitFrontmatter(Text, Lines, Out.BodyStart))
		{
			return Out;
		}
		Out.bPresent = true;
		for (const FString& Line : Lines)
		{
			FString Key, Value;
			if (Detail::SplitKeyValue(Line, Key, Value))
			{
				Out.Values.Emplace(Key, Value);
			}
		}
		return Out;
	}

	FString SetFrontmatterValues(const FString& Text, const TArray<TPair<FString, FString>>& Values)
	{
		TArray<FString> Lines;
		int32 BodyStart = 0;
		const bool bHadFrontmatter = Detail::SplitFrontmatter(Text, Lines, BodyStart);

		for (const TPair<FString, FString>& Pair : Values)
		{
			const int32 Existing = Lines.IndexOfByPredicate([&Pair](const FString& Line)
			{
				FString Key, Value;
				return Detail::SplitKeyValue(Line, Key, Value) && Key.Equals(Pair.Key, ESearchCase::IgnoreCase);
			});
			const FString Value = Pair.Value.TrimStartAndEnd();
			if (Value.IsEmpty())
			{
				if (Existing != INDEX_NONE)
				{
					Lines.RemoveAt(Existing);
				}
				continue;
			}
			const FString Line = FString::Printf(TEXT("%s: %s"), *Pair.Key, *Detail::QuoteValue(Value));
			if (Existing != INDEX_NONE)
			{
				Lines[Existing] = Line;
			}
			else
			{
				Lines.Add(Line);
			}
		}

		const FString Body = bHadFrontmatter ? Text.Mid(BodyStart) : Text;
		if (Lines.IsEmpty())
		{
			return Body;
		}
		return TEXT("---\n") + FString::Join(Lines, TEXT("\n")) + TEXT("\n---\n") + Body;
	}
}
