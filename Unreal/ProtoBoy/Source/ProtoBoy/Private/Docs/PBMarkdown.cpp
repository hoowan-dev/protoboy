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

		int32 LineStart = 0;
		while (LineStart <= Text.Len())
		{
			int32 LineEnd = Text.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, LineStart);
			if (LineEnd == INDEX_NONE)
			{
				LineEnd = Text.Len();
			}
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
}
