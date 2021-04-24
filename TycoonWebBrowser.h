// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WebBrowser.h"
#include "Engine/DataTable.h"
#include "TycoonWebBrowser.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInternalWebBrowser, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPageLoadStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTitleChanged, const FText&, NewTitle);

USTRUCT( BlueprintType )
struct FLocalURLWebPage : public FTableRowBase {
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local URL Webpage") FString Content;
};

/**
 * 
 */
UCLASS()
class DTTYCOON_API UTycoonWebBrowser : public UWebBrowser
{
	GENERATED_BODY()

	
	UFUNCTION()  bool OnLoadURLInternal(const FString& Method, const FString& Url, FString&  Response);
	UFUNCTION()  void OnLoadStartedInternal();
	UFUNCTION()  void OnTitleChangedInternal(const FText& InText);
	UFUNCTION()  void OnURLChangedInternal(const FText& InText);
public:
	// must be lower case
	UPROPERTY(BlueprintReadOnly, EditAnywhere) TArray<FString> WhitelistedURLs;
	// the url must equal the key (including trailing /), must be lower case
	UPROPERTY(BlueprintReadOnly, EditAnywhere) UDataTable* WebpageURLsExact;
	// the url must contain the row name, must be lower case, useful for files needed on multiple sites
	UPROPERTY(BlueprintReadOnly, EditAnywhere) UDataTable* WebpageURLsMatch;
	// page to show if not found, must be lower case
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FName PageNotFoundURL;
	
	// bind non permanent uobjects here
	UPROPERTY(BlueprintAssignable) FPageLoadStarted PageLoadStarted_Event;
	UPROPERTY(BlueprintAssignable) FTitleChanged TitleChanged_Event;
	
	UFUNCTION(BlueprintCallable) void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent);
	UFUNCTION(BlueprintCallable) void UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent);

	UFUNCTION(BlueprintCallable) void NavigateBack();
	UFUNCTION(BlueprintCallable) void NavigateForward();

	UFUNCTION(BlueprintNativeEvent) bool OnLoadURL(const FString& Method, const FString& Url, FString&  Response);

	UFUNCTION(BlueprintNativeEvent) TMap<FString, UObject*> GetObjectsForPage(const FString& Protocol, const FString& Address);

	void BindObjectsForPage();
protected:
	TSharedRef<SWidget> RebuildWidget() override;
};
