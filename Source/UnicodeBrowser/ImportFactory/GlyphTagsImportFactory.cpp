// all rights reserved
#include "GlyphTagsImportFactory.h"

#include "Factories/Factory.h"

#include "Misc/FileHelper.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UnicodeBrowser/DataAsset_FontTags.h"

UGlyphTagsImportFactory::UGlyphTagsImportFactory(FObjectInitializer const& ObjectInitializer) : Super(ObjectInitializer)
{
	// Factory
	SupportedClass = UDataAsset_FontTags::StaticClass();
	Formats.Add("json;Unicode Glyph Tags");
	bCreateNew = false;
	//bEditAfterNew = true;	
	bEditorImport = true;
	bText = true;
}

bool UGlyphTagsImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if(UDataAsset_FontTags const* Object = Cast<UDataAsset_FontTags>(Obj))
	{
		OutFilenames.Add(Object->SourceFile);
		return IsValid(Object) && FactoryCanImport(Object->SourceFile);
	}

	return false;
};

void UGlyphTagsImportFactory::SetReimportPaths(UObject* Obj, TArray<FString> const& NewReimportPaths)
{
	UDataAsset_FontTags* DataAsset_FontTags = Cast<UDataAsset_FontTags>(Obj);
	if (IsValid(DataAsset_FontTags))
	{
		DataAsset_FontTags->SourceFile = NewReimportPaths[0];
	}
}

bool UGlyphTagsImportFactory::FactoryCanImport(FString const& Filename)
{
	FString JsonString = "";
	
	if (!FFileHelper::LoadFileToString(JsonString, *Filename))
		return false;

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	auto const Reader = TJsonReaderFactory<>::Create(JsonString);
	FJsonSerializer::Deserialize(Reader, JsonObject);

	return JsonObject->HasTypedField<EJson::String>(TEXT("format")) && JsonObject->GetStringField(TEXT("format")) == TEXT("UnicodeBrowserGlyphTags_V1");
}

UObject* UGlyphTagsImportFactory::ImportObject(UClass* InClass, UObject* InOuter, FName const InName, EObjectFlags const InFlags, FString const& Filename, TCHAR const* Parms, bool& OutCanceled)
{
	UDataAsset_FontTags* Object = Cast<UDataAsset_FontTags>(UFactory::ImportObject(InClass, InOuter, InName, InFlags, Filename, Parms, OutCanceled));
	if (!OutCanceled && IsValid(Object))
	{
		if (Object->ImportFromJson(Filename))
		{
			return Object;
		}
	}

	return Object;
}

EReimportResult::Type UGlyphTagsImportFactory::Reimport(UObject* Obj)
{
	UDataAsset_FontTags* DataAsset_FontTags = Cast<UDataAsset_FontTags>(Obj);
	return IsValid(DataAsset_FontTags) && DataAsset_FontTags->ImportFromJson(DataAsset_FontTags->SourceFile) ? EReimportResult::Type::Succeeded : EReimportResult::Type::Failed;
};

UObject* UGlyphTagsImportFactory::FactoryCreateText(
	UClass* InClass,
	UObject* InParent,
	FName const InName,
	EObjectFlags const Flags,
	UObject* Context,
	TCHAR const* Type,
	TCHAR const*& Buffer,
	TCHAR const* BufferEnd,
	FFeedbackContext* Warn
)
{
	return NewObject<UDataAsset_FontTags>(InParent, InName, Flags);
}
