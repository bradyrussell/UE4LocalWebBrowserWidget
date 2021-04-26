// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WebBrowser.h"
#include "Engine/DataTable.h"
#include "CacheEngineSubsystem.h"
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

	// page to show if not found, relative to web root, must be lower case, extension required
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FString PageNotFoundURL;

	// relative to content folder. the url must equal the file path relative to WebRootDirectory, extension not required, must be lower case
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FString WebRootDirectory = "HTML/web/";
	// relative to content folder. the url must contain the file path relative to WebMatchDirectory, extension required, must be lower case, useful for files needed on multiple sites
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FString WebMatchDirectory = "HTML/match/";
	// relative to content folder. the template tag must contain the file path relative to WebTemplateDirectory, extension not required, must be lower case, useful for html needed on multiple pages
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FString WebTemplateDirectory = "HTML/template/";
	// if a directory is specified like test.com/, load the page for test.com/{this}
	UPROPERTY(BlueprintReadOnly, EditAnywhere) FString DefaultPageForDirectory = "index";
	
	UFUNCTION(BlueprintCallable) static UCacheEngineSubsystem* GetCacheSubsystem();
	UFUNCTION(BlueprintCallable) static void ClearCache(bool bIncludeWebMatch = true);
	
	// bind non permanent uobjects here
	UPROPERTY(BlueprintAssignable) FPageLoadStarted PageLoadStarted_Event;
	UPROPERTY(BlueprintAssignable) FTitleChanged TitleChanged_Event;
	
	UFUNCTION(BlueprintCallable) void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent);
	UFUNCTION(BlueprintCallable) void UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent);

	UFUNCTION(BlueprintCallable) void NavigateBack();
	UFUNCTION(BlueprintCallable) void NavigateForward();

	UFUNCTION(BlueprintNativeEvent) bool OnLoadURL(const FString& Method, const FString& Url, FString&  Response);

	UFUNCTION(BlueprintNativeEvent) TMap<FString, UObject*> GetObjectsForPage(const FString& Protocol, const FString& Address);
	
	// template tag <!--{NAME}-->
	bool ParseTemplates(FString& PageContent);

	void BindObjectsForPage();
protected:
	TSharedRef<SWidget> RebuildWidget() override;
};
