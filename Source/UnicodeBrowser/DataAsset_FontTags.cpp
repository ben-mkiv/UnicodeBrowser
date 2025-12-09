// all rights reserved

#include "DataAsset_FontTags.h"

#include "Dom/JsonObject.h"

#include "Engine/Font.h"

#include "Fonts/SlateFontInfo.h"

#include "Misc/FileHelper.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

TArray<FUnicodeCharacterTags>& UDataAsset_FontTags::GetCharactersMerged() const
{
	if (CharactersMerged.IsEmpty())
	{
		CharactersMerged = Characters;
		// create the codepoint cache for quicker lookup of existing entries
		CacheCodepoints();

		if (Parent && Parent != this) // there's a chance of infinite recursion here, this still doesn't catch recursion traps like A => B => A
		{
			for (FUnicodeCharacterTags& ParentCharacter : Parent->GetCharactersMerged())
			{
				// character exists, append tags
				if (int32 const* CharacterIndex = CodepointLookup.Find(ParentCharacter.Character))
				{
					for (FString& ParentCharacterTag : ParentCharacter.Tags)
					{
						CharactersMerged[*CharacterIndex].Tags.AddUnique(ParentCharacterTag);
					}
				}
				// character doesn't exist, add it
				else
				{
					CharactersMerged.Add(ParentCharacter);
				}
			}

			// recreate the codepoint cache if the parent has any characters
			if (Parent->GetCharactersMerged().Num() > 0)
			{
				CacheCodepoints();
			}
		}
	}
	return CharactersMerged;
}

TArray<int32> UDataAsset_FontTags::GetCharactersByNeedle(FString NeedleIn) const
{
	TArray<int32> Result;
	TArray<FString> Needles;

	// explode only if the length is >1 since "," is a valid character search term
	if (NeedleIn.Len() > 1)
	{
		NeedleIn.ParseIntoArray(Needles, TEXT(","));
	}
	else
	{
		Needles = {NeedleIn};
	}

	// trim, as the user may type stuff like "Phone, Calculator"
	for (FString& Needle : Needles)
	{
		Needle.TrimStartAndEndInline();
	}

	Result.Reserve(GetCharactersMerged().Num());
	for (FUnicodeCharacterTags const& Entry : GetCharactersMerged())
	{
		for (FString const& Needle : Needles)
		{
			if (Entry.ContainsNeedle(Needle))
			{
				Result.Add(Entry.Character);
				break;
			}
		}
	}

	return Result;
}

bool UDataAsset_FontTags::SupportsFont(FSlateFontInfo const& FontInfo) const
{
	// allow presets to apply to all fonts if they don't contain any specific fonts
	return Fonts.IsEmpty() || Fonts.Contains(Cast<UFont>(FontInfo.FontObject));
}

void UDataAsset_FontTags::CacheCodepoints() const
{
	CodepointLookup.Reset();
	CodepointLookup.Reserve(CharactersMerged.Num());

	for (int Idx = 0; Idx < CharactersMerged.Num(); Idx++)
	{
		CodepointLookup.Add(CharactersMerged[Idx].Character, Idx);
	}
}

TArray<FString> UDataAsset_FontTags::GetCodepointTags(int32 const Codepoint) const
{
	if (CodepointLookup.IsEmpty())
	{
		// this creates the cache
		// ReSharper disable once CppExpressionWithoutSideEffects
		GetCharactersMerged();
	}

	if (int32 const* Index = CodepointLookup.Find(Codepoint))
	{
		return GetCharactersMerged()[*Index].Tags;
	}

	return {};
}

bool UDataAsset_FontTags::ImportFromJson(FString Filename)
{
	FString JsonString = "";

	if (!FFileHelper::LoadFileToString(JsonString, *Filename)) return false;

	SourceFile = Filename;

	FString CodePointFieldDecimal = "";
	FString CodePointFieldHexadecimal = "";
	TArray<FString> TagFields;

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	auto const Reader = TJsonReaderFactory<>::Create(JsonString);
	FJsonSerializer::Deserialize(Reader, JsonObject);

	if (!JsonObject->HasTypedField<EJson::Array>(TEXT("tagFields")))
		return false;

	if(JsonObject->HasTypedField<EJson::String>(TEXT("codepointFieldDecimal")))
		JsonObject->TryGetStringField(TEXT("codepointFieldDecimal"), CodePointFieldDecimal);
	
	if(JsonObject->HasTypedField<EJson::String>(TEXT("codepointFieldHexadecimal")))
		JsonObject->TryGetStringField(TEXT("codepointFieldHexadecimal"), CodePointFieldHexadecimal);

	if (CodePointFieldDecimal.IsEmpty() && CodePointFieldHexadecimal.IsEmpty())
		return false;

	JsonObject->TryGetStringArrayField(TEXT("tagFields"), TagFields);
	if (TagFields.IsEmpty())
		return false;

	if (!JsonObject->HasTypedField<EJson::Array>(TEXT("glyphs")))
		return false;

	TArray<TSharedPtr<FJsonValue>> Glyphs = JsonObject->GetArrayField(TEXT("glyphs"));
	if (Glyphs.IsEmpty())
		return false;

	// flush current entries (e.g. on reimport there might be existing data)
	Characters.Empty();
	
	for (TSharedPtr<FJsonValue> const& GlyphValue : Glyphs)
	{
		TSharedPtr<FJsonObject> const Glyph = GlyphValue->AsObject();
		FUnicodeCharacterTags Data;

		if (CodePointFieldDecimal.Len() > 0)
		{
			if (Glyph->HasTypedField<EJson::Number>(*CodePointFieldDecimal))
			{
				Data.Character = Glyph->GetNumberField(*CodePointFieldDecimal);
			}
			else if (Glyph->HasTypedField<EJson::String>(*CodePointFieldDecimal))
			{
				FString DecimalString = Glyph->GetStringField(*CodePointFieldDecimal);
				Data.Character = FCString::Strtoi(*DecimalString, nullptr, 10);
			}
		}
		else if (CodePointFieldHexadecimal.Len() > 0)
		{
			if (!Glyph->HasTypedField<EJson::String>(*CodePointFieldHexadecimal))
				continue;

			FString HexString = Glyph->GetStringField(*CodePointFieldHexadecimal);
			if (!HexString.StartsWith("0x", ESearchCase::IgnoreCase))
				continue;

			Data.Character = FCString::Strtoi(*HexString, nullptr, 16);
		}

		for (FString& TagField : TagFields)
		{
			FString Tag = "";
			Glyph->TryGetStringField(*TagField, Tag);

			if (Tag.Len() > 0)
			{
				Data.Tags.Add(Tag);
			}
		}

		Characters.Add(Data);
	}

	return true;
}
