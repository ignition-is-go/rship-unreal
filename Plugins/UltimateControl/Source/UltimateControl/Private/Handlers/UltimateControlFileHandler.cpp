// Copyright Rocketship. All Rights Reserved.

#include "Handlers/UltimateControlFileHandler.h"
#include "UltimateControl.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FUltimateControlFileHandler::FUltimateControlFileHandler(UUltimateControlSubsystem* InSubsystem)
	: FUltimateControlHandlerBase(InSubsystem)
{
	RegisterMethod(
		TEXT("file.read"),
		TEXT("Read the contents of a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleRead));

	RegisterMethod(
		TEXT("file.write"),
		TEXT("Write content to a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleWrite),
		/* bIsDangerous */ true);

	RegisterMethod(
		TEXT("file.exists"),
		TEXT("Check if a file or directory exists"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleExists));

	RegisterMethod(
		TEXT("file.delete"),
		TEXT("Delete a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleDelete),
		/* bIsDangerous */ true,
		/* bRequiresConfirmation */ true);

	RegisterMethod(
		TEXT("file.list"),
		TEXT("List files in a directory"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleList));

	RegisterMethod(
		TEXT("file.getInfo"),
		TEXT("Get information about a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleGetInfo));

	RegisterMethod(
		TEXT("file.copy"),
		TEXT("Copy a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleCopy));

	RegisterMethod(
		TEXT("file.move"),
		TEXT("Move or rename a file"),
		TEXT("File"),
		FJsonRpcMethodHandler::CreateRaw(this, &FUltimateControlFileHandler::HandleMove),
		/* bIsDangerous */ true);
}

bool FUltimateControlFileHandler::ValidatePath(const FString& Path, FString& OutResolvedPath, TSharedPtr<FJsonObject>& OutError)
{
	// Resolve the path
	OutResolvedPath = Path;

	// Handle relative paths - make them relative to project
	// Note: IsRelativePath was deprecated in UE 5.6, use IsRelative instead
	if (FPaths::IsRelative(Path))
	{
		OutResolvedPath = FPaths::ProjectDir() / Path;
	}
	else
	{
		OutResolvedPath = Path;
	}

	// Normalize the path
	FPaths::NormalizeFilename(OutResolvedPath);
	FPaths::CollapseRelativeDirectories(OutResolvedPath);

	// Security check: ensure path is within project or engine directory
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString EngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
	FString ResolvedFull = FPaths::ConvertRelativePathToFull(OutResolvedPath);

	bool bInProject = ResolvedFull.StartsWith(ProjectDir);
	bool bInEngine = ResolvedFull.StartsWith(EngineDir);

	if (!bInProject && !bInEngine)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::Unauthorized,
			TEXT("Access denied: path must be within project or engine directory"));
		return false;
	}

	// Block access to sensitive directories
	TArray<FString> BlockedPatterns = {
		TEXT("Saved/Config"),
		TEXT("Intermediate"),
		TEXT(".git"),
		TEXT("Binaries"),
	};

	for (const FString& Pattern : BlockedPatterns)
	{
		if (ResolvedFull.Contains(Pattern))
		{
			OutError = UUltimateControlSubsystem::MakeError(
				EJsonRpcError::Unauthorized,
				FString::Printf(TEXT("Access denied: cannot access %s directories"), *Pattern));
			return false;
		}
	}

	return true;
}

bool FUltimateControlFileHandler::HandleRead(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	FString ResolvedPath;
	if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(ResolvedPath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("File not found: %s"), *Path));
		return false;
	}

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *ResolvedPath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to read file"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetStringField(TEXT("path"), Path);
	ResultObj->SetStringField(TEXT("content"), Content);
	ResultObj->SetNumberField(TEXT("size"), Content.Len());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleWrite(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	FString Content;

	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("content"), Content, OutError))
	{
		return false;
	}

	FString ResolvedPath;
	if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	bool bAppend = GetOptionalBool(Params, TEXT("append"), false);

	bool bSuccess;
	if (bAppend)
	{
		bSuccess = FFileHelper::SaveStringToFile(Content, *ResolvedPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
	}
	else
	{
		bSuccess = FFileHelper::SaveStringToFile(Content, *ResolvedPath);
	}

	if (!bSuccess)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::OperationFailed,
			TEXT("Failed to write file"));
		return false;
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), true);
	ResultObj->SetStringField(TEXT("path"), Path);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleExists(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	FString ResolvedPath;
	if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	bool bExists = FPaths::FileExists(ResolvedPath) || FPaths::DirectoryExists(ResolvedPath);
	bool bIsDirectory = FPaths::DirectoryExists(ResolvedPath);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("exists"), bExists);
	ResultObj->SetBoolField(TEXT("isDirectory"), bIsDirectory);
	ResultObj->SetStringField(TEXT("path"), Path);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleDelete(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	FString ResolvedPath;
	if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(ResolvedPath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("File not found: %s"), *Path));
		return false;
	}

	bool bSuccess = IFileManager::Get().Delete(*ResolvedPath);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleList(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path = GetOptionalString(Params, TEXT("path"), TEXT(""));
	FString Pattern = GetOptionalString(Params, TEXT("pattern"), TEXT("*"));
	bool bRecursive = GetOptionalBool(Params, TEXT("recursive"), false);

	FString ResolvedPath;
	if (Path.IsEmpty())
	{
		ResolvedPath = FPaths::ProjectDir();
	}
	else if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	if (!FPaths::DirectoryExists(ResolvedPath))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Directory not found: %s"), *Path));
		return false;
	}

	TArray<FString> Files;
	if (bRecursive)
	{
		IFileManager::Get().FindFilesRecursive(Files, *ResolvedPath, *Pattern, true, true);
	}
	else
	{
		IFileManager::Get().FindFiles(Files, *(ResolvedPath / Pattern), true, true);
	}

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	for (const FString& File : Files)
	{
		FString FullPath = bRecursive ? File : (ResolvedPath / File);

		TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
		FileObj->SetStringField(TEXT("name"), FPaths::GetCleanFilename(FullPath));
		FileObj->SetStringField(TEXT("path"), FullPath);
		FileObj->SetBoolField(TEXT("isDirectory"), FPaths::DirectoryExists(FullPath));

		if (!FPaths::DirectoryExists(FullPath))
		{
			int64 FileSize = IFileManager::Get().FileSize(*FullPath);
			FileObj->SetNumberField(TEXT("size"), static_cast<double>(FileSize));
		}

		FilesArray.Add(MakeShared<FJsonValueObject>(FileObj));
	}

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetArrayField(TEXT("files"), FilesArray);
	ResultObj->SetNumberField(TEXT("count"), FilesArray.Num());

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleGetInfo(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Path;
	if (!RequireString(Params, TEXT("path"), Path, OutError))
	{
		return false;
	}

	FString ResolvedPath;
	if (!ValidatePath(Path, ResolvedPath, OutError))
	{
		return false;
	}

	FFileStatData StatData = IFileManager::Get().GetStatData(*ResolvedPath);

	if (!StatData.bIsValid)
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("File not found: %s"), *Path));
		return false;
	}

	TSharedPtr<FJsonObject> InfoObj = MakeShared<FJsonObject>();
	InfoObj->SetStringField(TEXT("path"), Path);
	InfoObj->SetBoolField(TEXT("isDirectory"), StatData.bIsDirectory);
	InfoObj->SetBoolField(TEXT("isReadOnly"), StatData.bIsReadOnly);
	InfoObj->SetNumberField(TEXT("size"), static_cast<double>(StatData.FileSize));
	InfoObj->SetStringField(TEXT("creationTime"), StatData.CreationTime.ToString());
	InfoObj->SetStringField(TEXT("accessTime"), StatData.AccessTime.ToString());
	InfoObj->SetStringField(TEXT("modificationTime"), StatData.ModificationTime.ToString());
	InfoObj->SetStringField(TEXT("extension"), FPaths::GetExtension(ResolvedPath));
	InfoObj->SetStringField(TEXT("filename"), FPaths::GetCleanFilename(ResolvedPath));

	OutResult = MakeShared<FJsonValueObject>(InfoObj);
	return true;
}

bool FUltimateControlFileHandler::HandleCopy(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Source;
	FString Destination;

	if (!RequireString(Params, TEXT("source"), Source, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("destination"), Destination, OutError))
	{
		return false;
	}

	FString ResolvedSource;
	FString ResolvedDest;

	if (!ValidatePath(Source, ResolvedSource, OutError))
	{
		return false;
	}
	if (!ValidatePath(Destination, ResolvedDest, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(ResolvedSource))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Source file not found: %s"), *Source));
		return false;
	}

	uint32 CopyResult = IFileManager::Get().Copy(*ResolvedDest, *ResolvedSource);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), CopyResult == COPY_OK);
	ResultObj->SetStringField(TEXT("source"), Source);
	ResultObj->SetStringField(TEXT("destination"), Destination);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}

bool FUltimateControlFileHandler::HandleMove(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonValue>& OutResult, TSharedPtr<FJsonObject>& OutError)
{
	FString Source;
	FString Destination;

	if (!RequireString(Params, TEXT("source"), Source, OutError))
	{
		return false;
	}
	if (!RequireString(Params, TEXT("destination"), Destination, OutError))
	{
		return false;
	}

	FString ResolvedSource;
	FString ResolvedDest;

	if (!ValidatePath(Source, ResolvedSource, OutError))
	{
		return false;
	}
	if (!ValidatePath(Destination, ResolvedDest, OutError))
	{
		return false;
	}

	if (!FPaths::FileExists(ResolvedSource))
	{
		OutError = UUltimateControlSubsystem::MakeError(
			EJsonRpcError::NotFound,
			FString::Printf(TEXT("Source file not found: %s"), *Source));
		return false;
	}

	bool bSuccess = IFileManager::Get().Move(*ResolvedDest, *ResolvedSource);

	TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
	ResultObj->SetBoolField(TEXT("success"), bSuccess);
	ResultObj->SetStringField(TEXT("source"), Source);
	ResultObj->SetStringField(TEXT("destination"), Destination);

	OutResult = MakeShared<FJsonValueObject>(ResultObj);
	return true;
}
