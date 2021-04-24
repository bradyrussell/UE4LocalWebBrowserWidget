// Fill out your copyright notice in the Description page of Project Settings.

#include "TycoonWebBrowser.h"
#include "SWebBrowser.h"

DEFINE_LOG_CATEGORY(LogInternalWebBrowser);

bool UTycoonWebBrowser::OnLoadURLInternal(const FString& Method, const FString& Url, FString& Response) { return OnLoadURL(Method, Url, Response); }

void UTycoonWebBrowser::OnLoadStartedInternal() {
	if (PageLoadStarted_Event.IsBound())
		PageLoadStarted_Event.Broadcast();
	
}

void UTycoonWebBrowser::OnTitleChangedInternal(const FText& InText) {
	if (TitleChanged_Event.IsBound())
		TitleChanged_Event.Broadcast(InText);
}

void UTycoonWebBrowser::OnURLChangedInternal(const FText& InText) {
	// if we do this in the LoadStarted listener it doesnt get the proper url or even bind the object sometimes
	BindObjectsForPage();
	HandleOnUrlChanged(InText);
}

void UTycoonWebBrowser::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent) { if (WebBrowserWidget && Object) { WebBrowserWidget->BindUObject(Name, Object, bIsPermanent); } }

void UTycoonWebBrowser::UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent) {
	if (WebBrowserWidget && Object)
		WebBrowserWidget->UnbindUObject(Name, Object, bIsPermanent);
}

void UTycoonWebBrowser::NavigateBack() {
	if (WebBrowserWidget)
		WebBrowserWidget->GoBack();
}

void UTycoonWebBrowser::NavigateForward() {
	if (WebBrowserWidget)
		WebBrowserWidget->GoForward();
}

bool UTycoonWebBrowser::OnLoadURL_Implementation(const FString& Method, const FString& Url, FString& Response) {
	UE_LOG(LogInternalWebBrowser, Verbose, TEXT("Accessing URL: %s"), *Url);
	if (WhitelistedURLs.Contains(Url.ToLower())) {
		UE_LOG(LogInternalWebBrowser, Display, TEXT("Accessing whitelisted URL: %s"), *Url);
		return false;
	}

#if WITH_EDITOR
	checkf(WebpageURLsExact, TEXT("UTycoonWebBrowser::WebpageURLsExact was not valid!"))
	checkf(WebpageURLsMatch, TEXT("UTycoonWebBrowser::WebpageURLsMatch was not valid!"))
#endif
	if(!WebpageURLsExact || !WebpageURLsMatch) {
		UE_LOG(LogInternalWebBrowser, Display, TEXT("Webpage databases not set!"));
		Response = "No internet connection.";
		return true;
	}

	FString Protocol, Address;
	Url.ToLower().Split(TEXT("://"), &Protocol, &Address);

	/*if(bHTTPSRedirect && Protocol.Equals("http")) {
		Response = "<script>window.location.replace(\"https://"+Address+"\");</script>";
		return true;
	}*/
	
	// exact match
	if (WebpageURLsExact->GetRowNames().Contains(FName(Address))) {
		FLocalURLWebPage* Webpage = WebpageURLsExact->FindRow<FLocalURLWebPage>(FName(Address), TEXT("UTycoonWebBrowser::OnLoadURL_Implementation"), true);
		if (Webpage) {
			Response = Webpage->Content;
			UE_LOG(LogInternalWebBrowser, Display, TEXT("Exact URL: %s"), *Url);
			return true;
		}
	}

	// url contains row name
	TArray<FName> RowNames = WebpageURLsMatch->GetRowNames();
	for (auto RowName : RowNames) {
		if(Address.Contains(RowName.ToString())) {
			FLocalURLWebPage* Webpage = WebpageURLsMatch->FindRow<FLocalURLWebPage>(RowName, TEXT("UTycoonWebBrowser::OnLoadURL_Implementation"), true);
			if (Webpage) {
				Response = Webpage->Content;
				UE_LOG(LogInternalWebBrowser, Display, TEXT("Matched URL: %s contains %s"), *Url, *RowName.ToString());
				return true;
			}
			break; 
		}
	}

	UE_LOG(LogInternalWebBrowser, Display, TEXT("URL not found in whitelist or database: %s"), *Url);
	
	if (WebpageURLsExact->GetRowNames().Contains(PageNotFoundURL)) {
		FLocalURLWebPage* Webpage = WebpageURLsExact->FindRow<FLocalURLWebPage>(PageNotFoundURL, TEXT("UTycoonWebBrowser::OnLoadURL_Implementation"), true);
		if (Webpage) {
			Response = Webpage->Content;
			//UE_LOG(LogInternalWebBrowser, Display, TEXT("URL not found in whitelist or database: %s"), *Url);
			return true;
		}
	}
	
	Response = "This page could not be found, and an error page was not set.";
	UE_LOG(LogInternalWebBrowser, Warning, TEXT("URL not found in whitelist or database and error page URL was not set: %s"), *Url);
	return true;
}

TMap<FString, UObject*> UTycoonWebBrowser::GetObjectsForPage_Implementation(const FString& Protocol, const FString& Address) {
	return TMap<FString, UObject*>();
}

void UTycoonWebBrowser::BindObjectsForPage() {
	if (!WebBrowserWidget) return;
	const FString Url = WebBrowserWidget->GetUrl();
	FString Protocol, Address;
	Url.ToLower().Split(TEXT("://"), &Protocol, &Address);

	TMap<FString, UObject*> ObjectsForPage = GetObjectsForPage(Protocol, Address);

	for (auto Object : ObjectsForPage) {
		BindUObject(Object.Key, Object.Value, false);
		UE_LOG(LogInternalWebBrowser, Display, TEXT("Bound object %s to URL: %s"), *Object.Key, *Url);
	}
}

TSharedRef<SWidget> UTycoonWebBrowser::RebuildWidget() {
	if (IsDesignTime()) {
		return SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Tycoon Web Browser"))
		];
	}
	else {
		WebBrowserWidget = SNew(SWebBrowser)
			.InitialURL(InitialURL)
			.ShowControls(false)
			.SupportsTransparency(bSupportsTransparency)
			.OnUrlChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, OnURLChangedInternal))
			.OnBeforePopup(BIND_UOBJECT_DELEGATE(FOnBeforePopupDelegate, HandleOnBeforePopup))
			.OnLoadUrl(BIND_UOBJECT_DELEGATE(SWebBrowser::FOnLoadUrl, OnLoadURLInternal))
			.OnLoadStarted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, OnLoadStartedInternal))
			.OnTitleChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, OnTitleChangedInternal));

		return WebBrowserWidget.ToSharedRef();
	}
}
