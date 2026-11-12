#pragma once

#include "Dom/JsonObject.h"
#include "MCP/PBMCPServer.h"

/**
 * Helpers for writing MCP tools: input schemas and argument access.
 * Shared by the tool registries (MCP/PBMCPTools.cpp, Sourcing/PBSourcingMCP.cpp, ...); call them qualified.
 */
namespace ProtoBoyMCPTools
{
	struct FParam
	{
		const TCHAR* Name;
		const TCHAR* Type; // "string", "integer" or "boolean"
		const TCHAR* Description;
		bool bRequired = false;
		TArray<FString> Enum;
	};

	inline TSharedPtr<FJsonObject> MakeSchema(const TArray<FParam>& Params)
	{
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Required;
		for (const FParam& Param : Params)
		{
			TSharedRef<FJsonObject> Property = MakeShared<FJsonObject>();
			Property->SetStringField(TEXT("type"), Param.Type);
			Property->SetStringField(TEXT("description"), Param.Description);
			if (!Param.Enum.IsEmpty())
			{
				TArray<TSharedPtr<FJsonValue>> Values;
				for (const FString& Value : Param.Enum)
				{
					Values.Add(MakeShared<FJsonValueString>(Value));
				}
				Property->SetArrayField(TEXT("enum"), Values);
			}
			Properties->SetObjectField(Param.Name, Property);
			if (Param.bRequired)
			{
				Required.Add(MakeShared<FJsonValueString>(Param.Name));
			}
		}

		TSharedRef<FJsonObject> Schema = MakeShared<FJsonObject>();
		Schema->SetStringField(TEXT("type"), TEXT("object"));
		Schema->SetObjectField(TEXT("properties"), Properties);
		if (!Required.IsEmpty())
		{
			Schema->SetArrayField(TEXT("required"), Required);
		}
		Schema->SetBoolField(TEXT("additionalProperties"), false);
		return Schema;
	}

	inline FString GetString(const FProtoBoyMCPToolCall& Call, const TCHAR* Name)
	{
		FString Value;
		Call.Args->TryGetStringField(Name, Value);
		return Value.TrimStartAndEnd();
	}

	inline bool GetBool(const FProtoBoyMCPToolCall& Call, const TCHAR* Name, bool bDefault)
	{
		bool bValue = bDefault;
		Call.Args->TryGetBoolField(Name, bValue);
		return bValue;
	}

	inline int32 GetInt(const FProtoBoyMCPToolCall& Call, const TCHAR* Name, int32 Default, int32 Min, int32 Max)
	{
		int32 Value = Default;
		Call.Args->TryGetNumberField(Name, Value);
		return FMath::Clamp(Value, Min, Max);
	}

	/** Returns false (and finishes the call with an error) if a required string argument is missing. */
	inline bool Require(const FProtoBoyMCPToolCall& Call, const FProtoBoyMCPToolDone& Done, std::initializer_list<const TCHAR*> Names)
	{
		for (const TCHAR* Name : Names)
		{
			if (GetString(Call, Name).IsEmpty())
			{
				Done(FProtoBoyMCPToolResult::Error(FString::Printf(TEXT("Missing required argument \"%s\"."), Name)));
				return false;
			}
		}
		return true;
	}

	/** A registered tool from its parts. */
	inline FProtoBoyMCPTool MakeTool(const TCHAR* Name, const FText& Title, const FText& Description, bool bWrites,
		const TArray<FParam>& Params, FProtoBoyMCPToolHandler Handler, TFunction<bool()> IsAvailable = nullptr)
	{
		FProtoBoyMCPTool Tool;
		Tool.Name = FName(Name);
		Tool.Title = Title;
		Tool.Description = Description;
		Tool.bWrites = bWrites;
		Tool.InputSchema = MakeSchema(Params);
		Tool.Handler = MoveTemp(Handler);
		Tool.IsAvailable = MoveTemp(IsAvailable);
		return Tool;
	}
}
