#include "RocketARSetupActorDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

static bool bShowOnlyRocketAR = false;

TSharedRef<IDetailCustomization> FRocketARSetupActorDetails::MakeInstance()
{
	return MakeShareable(new FRocketARSetupActorDetails);
}

void FRocketARSetupActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Categories to hide when the filter is active
	static const FName HiddenCategories[] = {
		TEXT("Actor"),
		TEXT("Rendering"),
		TEXT("Replication"),
		TEXT("Collision"),
		TEXT("Input"),
		TEXT("HLOD"),
		TEXT("Cooking"),
		TEXT("Networking"),
		TEXT("Physics"),
		TEXT("Tags"),
		TEXT("AssetUserData"),
		TEXT("LOD"),
		TEXT("Tick"),
		TEXT("ComponentTick"),
		TEXT("ComponentReplication"),
		TEXT("Activation"),
		TEXT("Sockets"),
		TEXT("Variable"),
		TEXT("WorldPartition"),
		TEXT("DataLayers"),
		TEXT("Navigation"),
		TEXT("TransformCommon"),
	};

	// Add filter toggle at the top
	IDetailCategoryBuilder& FilterCat = DetailBuilder.EditCategory(
		TEXT("RocketAR Filter"), FText::GetEmpty(), ECategoryPriority::Important);

	// Capture a pointer to the builder for the lambda
	IDetailLayoutBuilder* BuilderPtr = &DetailBuilder;

	FilterCat.AddCustomRow(FText::FromString(TEXT("Filter")))
		.WholeRowContent()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.OnClicked_Lambda([BuilderPtr]()
			{
				bShowOnlyRocketAR = !bShowOnlyRocketAR;
				if (BuilderPtr)
				{
					BuilderPtr->ForceRefreshDetails();
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text_Lambda([]()
				{
					return bShowOnlyRocketAR
						? FText::FromString(TEXT("Show All Properties"))
						: FText::FromString(TEXT("Show Only RocketAR"));
				})
			]
		];

	if (bShowOnlyRocketAR)
	{
		for (const FName& Cat : HiddenCategories)
		{
			DetailBuilder.HideCategory(Cat);
		}
	}
}
