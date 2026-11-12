#include "Tasks/PBTaskProvider.h"

namespace ProtoBoyTasks
{
	const TCHAR* DesignDocPrefix = TEXT("Design doc: ");

	FString StatusToString(EProtoBoyTaskStatus Status)
	{
		switch (Status)
		{
		case EProtoBoyTaskStatus::ToDo: return TEXT("todo");
		case EProtoBoyTaskStatus::InProgress: return TEXT("in_progress");
		case EProtoBoyTaskStatus::Done: return TEXT("done");
		case EProtoBoyTaskStatus::Other: break;
		}
		return TEXT("other");
	}

	TOptional<EProtoBoyTaskStatus> StatusFromString(const FString& Value)
	{
		const FString V = Value.TrimStartAndEnd().ToLower().Replace(TEXT(" "), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
		if (V == TEXT("todo") || V == TEXT("to_do")) { return EProtoBoyTaskStatus::ToDo; }
		if (V == TEXT("in_progress") || V == TEXT("doing")) { return EProtoBoyTaskStatus::InProgress; }
		if (V == TEXT("done")) { return EProtoBoyTaskStatus::Done; }
		return {};
	}

	FString MakeDesignDocLine(const FString& DocPath)
	{
		return FString(DesignDocPrefix) + DocPath.TrimStartAndEnd();
	}

	FString FindDesignDoc(const FString& Description)
	{
		TArray<FString> Lines;
		Description.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			FString Trimmed = Line.TrimStartAndEnd();
			// Tolerate Markdown emphasis or list markers around the label.
			Trimmed.ReplaceInline(TEXT("*"), TEXT(""));
			Trimmed.RemoveFromStart(TEXT("- "));
			if (Trimmed.StartsWith(DesignDocPrefix, ESearchCase::IgnoreCase))
			{
				return Trimmed.Mid(FCString::Strlen(DesignDocPrefix)).TrimStartAndEnd();
			}
		}
		return FString();
	}
}
