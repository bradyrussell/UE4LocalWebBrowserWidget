// Fill out your copyright notice in the Description page of Project Settings.

#include "TycoonWebBrowser.h"
#include "SWebBrowser.h"
#include "HAL/FileManager.h"
#include "HAL/FileManagerGeneric.h"

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

UCacheEngineSubsystem* UTycoonWebBrowser::GetCacheSubsystem() { return GEngine->GetEngineSubsystem<UCacheEngineSubsystem>(); }

void UTycoonWebBrowser::ClearCache(bool bIncludeWebMatch) {
	UCacheEngineSubsystem* CacheSubsystem = GetCacheSubsystem();
	if (CacheSubsystem)
		CacheSubsystem->ClearCache(bIncludeWebMatch);
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
	UE_LOG(LogInternalWebBrowser, Display, TEXT("Accessing URL: %s"), *Url); //todo verbose

	if (WhitelistedURLs.Contains(Url.ToLower())) {
		UE_LOG(LogInternalWebBrowser, Display, TEXT("Accessing whitelisted URL: %s"), *Url);
		return false;
	}

	FString Protocol, Address;
	Url.ToLower().Split(TEXT("://"), &Protocol, &Address);

	//remove trailing slash
	if (Address.EndsWith("/"))
		Address.LeftChopInline(1);

	// check both caches before hitting files
	UCacheEngineSubsystem* CacheSubsystem = GetCacheSubsystem();
	if (CacheSubsystem) {
		// cached exact 
		if (CacheSubsystem->WebRootCache.Contains(Address)) {
			Response = CacheSubsystem->WebRootCache.FindChecked(Address);
			UE_LOG(LogInternalWebBrowser, Display, TEXT("Cached Exact URL: %s"), *Url);
			return true;
		}

		// cached match
		TArray<FString> Keys;
		CacheSubsystem->WebMatchCache.GetKeys(Keys);
		for (auto Key : Keys) {
			if (Address.Contains(Key)) {
				Response = CacheSubsystem->WebMatchCache.FindChecked(Key);
				UE_LOG(LogInternalWebBrowser, Display, TEXT("Cached Matched URL: %s contains %s"), *Url, *Key);
				return true;
			}
		}
	}
	else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("Cache subsystem was not available!")); }
	// not in cache

	FString ErrorPageDir = "";

	// file exists with extension
	if (!WebRootDirectory.IsEmpty()) {
		const FString WebRootDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), WebRootDirectory));

		if (FPaths::DirectoryExists(WebRootDir)) {
			FString ExactPagePath = FPaths::Combine(WebRootDir, Address);
			ErrorPageDir = FPaths::Combine(WebRootDir, PageNotFoundURL);
			if (FPaths::FileExists(ExactPagePath)) {
				if (FFileHelper::LoadFileToString(Response, *ExactPagePath)) {
					if (CacheSubsystem)
						CacheSubsystem->WebRootCache.Add(Address, Response);
					UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded file from disk for URL: %s from file %s"), *Url, *ExactPagePath);
					return true;
				}
				UE_LOG(LogInternalWebBrowser, Warning, TEXT("Failed to load file: %s"), *ExactPagePath);
			}

			// index redirect
			if (FPaths::DirectoryExists(ExactPagePath)) {
				UE_LOG(LogInternalWebBrowser, Warning, TEXT("Index redirect: %s"), *ExactPagePath);
				ExactPagePath = FPaths::Combine(ExactPagePath, DefaultPageForDirectory);
				UE_LOG(LogInternalWebBrowser, Warning, TEXT("To: %s"), *ExactPagePath);
			}

			// file exists but address is without extension?
			if (FPaths::GetExtension(ExactPagePath).Equals(TEXT(""))) {
				TArray<FString> OutFiles;
				const FString SearchWildcard = ExactPagePath + "*";
				IFileManager::Get().FindFiles(OutFiles, *SearchWildcard, true, false);

				if (OutFiles.Num() > 0) {
					FString ParentDirectory, Filename, Extension;
					FPaths::Split(ExactPagePath, ParentDirectory, Filename, Extension);

					for (auto OutFile : OutFiles) {
						FString OutPath = FPaths::Combine(ParentDirectory, OutFile);
						if (FPaths::FileExists(OutPath)) {
							if (FFileHelper::LoadFileToString(Response, *OutPath)) {
								if (!ParseTemplates(Response)) { UE_LOG(LogInternalWebBrowser, Display, TEXT("Failed to parse templates for URL: %s"), *Url); }
								if (CacheSubsystem)
									CacheSubsystem->WebRootCache.Add(Address, Response);
								UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded file from disk for URL: %s from file %s"), *Url, *OutPath);
								return true;
							}
							UE_LOG(LogInternalWebBrowser, Warning, TEXT("Failed to load file: %s"), *OutPath);
						}
						else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("File does not exist: %s"), *OutPath); }
					}
				}
			}
		}
		else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebRootDirectory %s does not exist!"), *WebRootDir); }
	}
	else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebRootDirectory is not set!")); }

	if (!WebMatchDirectory.IsEmpty()) {
		const FString WebMatchDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), WebMatchDirectory));

		if (FPaths::DirectoryExists(WebMatchDir)) {
			TArray<FString> MatchFileNames;
			if(CacheSubsystem && CacheSubsystem->MatchFileNameCache.Num() > 0) {
				MatchFileNames = CacheSubsystem->MatchFileNameCache;
			} else {
				IFileManager::Get().FindFilesRecursive(MatchFileNames, *WebMatchDir, TEXT("*"), true, false);
				if(CacheSubsystem) CacheSubsystem->MatchFileNameCache = MatchFileNames;
			}
			
			//UE_LOG(LogInternalWebBrowser, Warning, TEXT("Search for match file yielded %d results: %s"), MatchFileNames.Num(), *WebMatchDir);
			for (auto OutFile : MatchFileNames) {
				FString ParentDirectory, Filename, Extension;
				FPaths::Split(OutFile, ParentDirectory, Filename, Extension);
				//UE_LOG(LogInternalWebBrowser, Warning, TEXT("Match file parent: %s, name: %s, to %s"), *ParentDirectory, *(Filename+"."+Extension), *Address);
				auto SearchKey = Filename + "." + Extension; // todo should include directories after Match Root
				if (Address.Contains(SearchKey)) {
					UE_LOG(LogInternalWebBrowser, Warning, TEXT("Match file : %s, %s"), *Address, *OutFile);
					if (FPaths::FileExists(OutFile)) {
						if (FFileHelper::LoadFileToString(Response, *OutFile)) {
							if (!ParseTemplates(Response)) { UE_LOG(LogInternalWebBrowser, Display, TEXT("Failed to parse templates for URL: %s"), *Url); }
							if (CacheSubsystem)
								CacheSubsystem->WebMatchCache.Add(SearchKey, Response);
							UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded file from disk for URL: %s from file %s"), *Url, *OutFile);
							return true;
						}
						UE_LOG(LogInternalWebBrowser, Warning, TEXT("Failed to load file: %s"), *OutFile);
					}
					else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("File does not exist: %s"), *OutFile); }
				}
			}
		}
		else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebMatchDirectory %s does not exist!"), *WebMatchDir); }
	}
	else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebMatchDirectory is not set!")); }

	// todo cache failed urls?
	//do error page
	if (!ErrorPageDir.IsEmpty()) {
		if (CacheSubsystem && CacheSubsystem->WebRootCache.Contains(PageNotFoundURL)) {
			Response = CacheSubsystem->WebRootCache.FindChecked(PageNotFoundURL);
			UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded cached error page URL: %s"), *Url);
			return true;
		}
		else {
			if (FPaths::FileExists(ErrorPageDir)) {
				if (FFileHelper::LoadFileToString(Response, *ErrorPageDir)) {
					if (!ParseTemplates(Response)) { UE_LOG(LogInternalWebBrowser, Display, TEXT("Failed to parse templates for URL: %s"), *Url); }
					if (CacheSubsystem)
						CacheSubsystem->WebRootCache.Add(PageNotFoundURL, Response);
					UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded file from disk for error page URL: %s from file %s"), *Url, *ErrorPageDir);
					return true;
				}
				UE_LOG(LogInternalWebBrowser, Warning, TEXT("Failed to load file: %s"), *ErrorPageDir);
			}
			else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("File does not exist: %s"), *ErrorPageDir); }
		}
	}
	else { UE_LOG(LogInternalWebBrowser, Warning, TEXT("No error page was set or the web root directory does not exist."), *ErrorPageDir); }

	Response = "This page could not be found, and an error page was not set.";
	UE_LOG(LogInternalWebBrowser, Warning, TEXT("URL not found in whitelist or database and error page URL was not set: %s"), *Url);
	return true;
}

TMap<FString, UObject*> UTycoonWebBrowser::GetObjectsForPage_Implementation(const FString& Protocol, const FString& Address) { return TMap<FString, UObject*>(); }

bool UTycoonWebBrowser::ParseTemplates(FString& PageContent) {
	UCacheEngineSubsystem* CacheSubsystem = GetCacheSubsystem();

	TArray<FString> CachedKeys;
	if (CacheSubsystem) {
		CacheSubsystem->WebTemplateCache.GetKeys(CachedKeys);

		for (auto OutKey : CachedKeys) {
			if (PageContent.Contains(OutKey)) {
				UE_LOG(LogInternalWebBrowser, Warning, TEXT("Match cached template file : %s"), *OutKey);

				FString Content = *CacheSubsystem->WebTemplateCache.Find(OutKey);
				PageContent.ReplaceInline(*OutKey, *Content);
				UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded template file from cache: %s"), *OutKey);
				continue;
			}
		}
	}

	if (!WebTemplateDirectory.IsEmpty()) {
		const FString WebTemplateDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), WebTemplateDirectory));
		
		if (FPaths::DirectoryExists(WebTemplateDir)) {
			TArray<FString> TemplateFileNames;
			if(CacheSubsystem && CacheSubsystem->TemplateFileNameCache.Num() > 0) {
				TemplateFileNames = CacheSubsystem->TemplateFileNameCache;
			} else {
				IFileManager::Get().FindFilesRecursive(TemplateFileNames, *WebTemplateDir, TEXT("*"), true, false);
				if(CacheSubsystem) CacheSubsystem->TemplateFileNameCache = TemplateFileNames;
			}
			
			for (auto OutFile : TemplateFileNames) {
				FString ParentDirectory, Filename, Extension;
				FPaths::Split(OutFile, ParentDirectory, Filename, Extension);
				auto SearchKeyWithExtension = "<!--{" + Filename + "." + Extension + "}-->"; // todo should include directories after template Root
				auto SearchKeyWithoutExtension = "<!--{" + Filename + "}-->";

				if (CachedKeys.Contains(SearchKeyWithExtension) || CachedKeys.Contains(SearchKeyWithoutExtension))
					continue;

				if (PageContent.Contains(SearchKeyWithExtension) || PageContent.Contains(SearchKeyWithoutExtension)) {
					UE_LOG(LogInternalWebBrowser, Warning, TEXT("Match template file : %s"), *OutFile);
					if (FPaths::FileExists(OutFile)) {
						FString TemplateContents;
						if (FFileHelper::LoadFileToString(TemplateContents, *OutFile)) {
							auto Key = PageContent.Contains(SearchKeyWithExtension) ? SearchKeyWithExtension : SearchKeyWithoutExtension;
							if (CacheSubsystem) { CacheSubsystem->WebTemplateCache.Add(Key, TemplateContents); }
							PageContent.ReplaceInline(*Key, *TemplateContents);
							UE_LOG(LogInternalWebBrowser, Display, TEXT("Loaded template file %s"), *OutFile);
							continue;
						}
						UE_LOG(LogInternalWebBrowser, Warning, TEXT("Failed to load file: %s"), *OutFile);
						return false;
					}
					else {
						UE_LOG(LogInternalWebBrowser, Warning, TEXT("File does not exist: %s"), *OutFile);
						return false;
					}
				}
			}
		}
		else {
			UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebTemplateDirectory %s does not exist!"), *WebTemplateDir);
			return false;
		}
	}
	else {
		UE_LOG(LogInternalWebBrowser, Warning, TEXT("WebTemplateDirectory is not set!"));
		return false;
	}
	return true;
}

void UTycoonWebBrowser::BindObjectsForPage() {
	if (!WebBrowserWidget)
		return;
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
