#pragma once
#include "Fonts/FontMeasure.h"
#include "Fonts/UnicodeBlockRange.h"

#include "Framework/Application/SlateApplication.h"
#include "Internationalization/TextChar.h"

class FUnicodeBrowserRow : public TSharedFromThis<FUnicodeBrowserRow>
{
public:
	FUnicodeBrowserRow(int32 const CodePointIn, TOptional<EUnicodeBlockRange> BlockRangeIn, FSlateFontInfo const* FontInfoIn = nullptr) :
		Codepoint(CodePointIn),
		BlockRange(BlockRangeIn),
		FontInfo(FontInfoIn)
	{
		if(Codepoint != -1)
		{
			bHasValidCharacter = FTextChar::ConvertCodepointToString(Codepoint, Character);	
		}		
	}

	FString Character;
	bool bHasValidCharacter = false;
	int32 Codepoint = -1;
	TOptional<EUnicodeBlockRange> BlockRange;

	bool bFilteredByTag = false;

	FSlateFontInfo const* FontInfo = nullptr;

private:
	mutable FFontData const* FontData = nullptr;
	mutable TOptional<FVector2D> Measurements;
	mutable TOptional<bool> bCanLoadCodepoint;
	mutable TOptional<float> ScalingFactor;

public:
	FFontData const* GetFontData() const
	{
		if (!FontData && FontInfo)
		{
			float ScalingFactorResult;
			FontData = &FSlateApplication::Get().GetRenderer()->GetFontCache()->GetFontDataForCodepoint(*FontInfo, Codepoint, ScalingFactorResult);
			ScalingFactor = ScalingFactorResult;
		}

		return FontData;
	}

	bool CanLoadCodepoint() const
	{
		if (!bCanLoadCodepoint.IsSet())
		{
			if (FontInfo && ensureAlways(GetFontData()))
			{
				bCanLoadCodepoint = FSlateApplication::Get().GetRenderer()->GetFontCache()->CanLoadCodepoint(*FontData, Codepoint);
			}
		}

		return bCanLoadCodepoint.Get(false);
	}

	FVector2D GetMeasurements() const
	{
		if (!Measurements.IsSet() && FontInfo)
		{
			Measurements = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(*Character, *FontInfo);
		}

		return Measurements.Get(FVector2D::ZeroVector);
	}

	float GetScaling() const
	{
		if (!ScalingFactor.IsSet())
		{
			// this will try to populate the ScalingFactor
			// ReSharper disable once CppExpressionWithoutSideEffects
			GetFontData();
		}

		return ScalingFactor.Get(0.0f);
	}

	// preload cached data
	void Preload() const
	{
		// ReSharper disable once CppExpressionWithoutSideEffects
		GetFontData();
		// ReSharper disable once CppExpressionWithoutSideEffects
		GetMeasurements();
		// ReSharper disable once CppExpressionWithoutSideEffects
		CanLoadCodepoint();
	}

	friend bool operator==(FUnicodeBrowserRow const& Lhs, FUnicodeBrowserRow const& RHS)
	{
		return Lhs.Codepoint == RHS.Codepoint
			&& Lhs.Character == RHS.Character
			&& Lhs.BlockRange == RHS.BlockRange
			&& Lhs.FontData == RHS.FontData
			&& Lhs.ScalingFactor == RHS.ScalingFactor
			&& Lhs.Measurements == RHS.Measurements
			&& Lhs.bCanLoadCodepoint == RHS.bCanLoadCodepoint;
	}

	friend bool operator!=(FUnicodeBrowserRow const& Lhs, FUnicodeBrowserRow const& RHS) { return !(Lhs == RHS); }
};
