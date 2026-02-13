// Copyright Rocketship. All Rights Reserved.

#include "SRship2110MappingPanel.h"

#include "RshipSubsystem.h"
#include "RshipContentMappingManager.h"
#include "RshipSettings.h"

#if RSHIP_EDITOR_HAS_2110
#include "Rship2110.h"
#include "Rship2110Subsystem.h"
#include "Rivermax/Rship2110VideoSender.h"
#include "Rship2110Types.h"
#endif

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/DefaultValueHelper.h"

#define LOCTEXT_NAMESPACE "SRship2110MappingPanel"

void SRship2110MappingPanel::Construct(const FArguments& InArgs)
{
	TimeSinceLastRefresh = 0.0f;

	ChildSlot
	[
		SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				BuildOverviewSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				BuildStreamListSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				BuildContextListSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				BuildBindingSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SSeparator)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildSelectionDetailsSection()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				BuildUserGuideSection()
			]
		]
	];

	RefreshPanel();
}

void SRship2110MappingPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh >= RefreshInterval)
	{
		TimeSinceLastRefresh = 0.0f;
		RefreshPanel();
	}
}

void SRship2110MappingPanel::RefreshPanel()
{
	RefreshSubsystemState();
	RefreshStreams();
	RefreshContexts();
	ReconcileSelection();
	UpdateSummaries();
	UpdateSelectionDetails();
	UpdateBindingInputsFromSelection();
}

bool SRship2110MappingPanel::Is2110RuntimeAvailable() const
{
#if RSHIP_EDITOR_HAS_2110
	return FRship2110Module::IsAvailable();
#else
	return false;
#endif
}

bool SRship2110MappingPanel::IsContentMappingAvailable() const
{
	if (!Is2110RuntimeAvailable())
	{
		return false;
	}

	const URshipSettings* Settings = GetDefault<URshipSettings>();
	if (!Settings || !Settings->bEnableContentMapping)
	{
		return false;
	}

	return GetContentMappingManager() != nullptr;
}

void SRship2110MappingPanel::RefreshSubsystemState()
{
	if (ModuleStatusText.IsValid())
	{
		if (Is2110RuntimeAvailable())
		{
			ModuleStatusText->SetText(LOCTEXT("ModuleAvailable", "SMPTE 2110 runtime: Available"));
			ModuleStatusText->SetColorAndOpacity(FLinearColor::Green);
		}
		else
		{
			ModuleStatusText->SetText(LOCTEXT("ModuleMissing", "SMPTE 2110 runtime: Not available"));
			ModuleStatusText->SetColorAndOpacity(FLinearColor::Red);
		}
	}

	if (ContentMappingStatusText.IsValid())
	{
		if (IsContentMappingAvailable())
		{
			ContentMappingStatusText->SetText(LOCTEXT("MappingAvailable", "Content Mapping: Enabled"));
			ContentMappingStatusText->SetColorAndOpacity(FLinearColor::Green);
		}
		else
		{
			ContentMappingStatusText->SetText(LOCTEXT("MappingUnavailable", "Content Mapping: Disabled"));
			ContentMappingStatusText->SetColorAndOpacity(FLinearColor::Orange);
		}
	}
}

URship2110Subsystem* SRship2110MappingPanel::Get2110Subsystem() const
{
#if RSHIP_EDITOR_HAS_2110
	return GEngine ? GEngine->GetEngineSubsystem<URship2110Subsystem>() : nullptr;
#else
	return nullptr;
#endif
}

URshipSubsystem* SRship2110MappingPanel::GetRshipSubsystem() const
{
	return GEngine ? GEngine->GetEngineSubsystem<URshipSubsystem>() : nullptr;
}

URshipContentMappingManager* SRship2110MappingPanel::GetContentMappingManager() const
{
	if (URshipSubsystem* Subsystem = GetRshipSubsystem())
	{
		return Subsystem->GetContentMappingManager();
	}
	return nullptr;
}

void SRship2110MappingPanel::RefreshStreams()
{
	StreamItems.Empty();
	BoundContextCounts.Empty();

	URship2110Subsystem* Subsystem = Get2110Subsystem();
	if (!Subsystem)
	{
		if (StreamListView.IsValid())
		{
			StreamListView->RequestListRefresh();
		}
		return;
	}

	const TArray<FString> StreamIds = Subsystem->GetActiveStreamIds();
	for (const FString& StreamId : StreamIds)
	{
		TSharedPtr<FRship2110MappingStreamItem> Item = MakeShared<FRship2110MappingStreamItem>();
		Item->StreamId = StreamId;

#if RSHIP_EDITOR_HAS_2110
		URship2110VideoSender* Sender = Subsystem->GetVideoSender(StreamId);
		if (!Sender)
		{
			Item->StateText = TEXT("NotFound");
			Item->StateColor = FLinearColor::Red;
			Item->bStreamMissing = true;
		}
		else
		{
			ERship2110StreamState State = Sender->GetState();
			switch (State)
			{
			case ERship2110StreamState::Stopped:
				Item->StateText = TEXT("Stopped");
				Item->StateColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
				break;
			case ERship2110StreamState::Starting:
				Item->StateText = TEXT("Starting");
				Item->StateColor = FLinearColor(1.0f, 0.8f, 0.0f, 1.0f);
				break;
			case ERship2110StreamState::Running:
				Item->StateText = TEXT("Running");
				Item->StateColor = FLinearColor(0.0f, 0.95f, 0.0f, 1.0f);
				Item->bIsRunning = true;
				break;
			case ERship2110StreamState::Paused:
				Item->StateText = TEXT("Paused");
				Item->StateColor = FLinearColor(1.0f, 0.6f, 0.0f, 1.0f);
				break;
			case ERship2110StreamState::Error:
				Item->StateText = TEXT("Error");
				Item->StateColor = FLinearColor(1.0f, 0.15f, 0.15f, 1.0f);
				break;
			default:
				Item->StateText = TEXT("Unknown");
				Item->StateColor = FLinearColor::White;
				break;
			}

			const FRship2110VideoFormat Format = Sender->GetVideoFormat();
			Item->Resolution = FString::Printf(TEXT("%dx%d"), Format.Width, Format.Height);
			Item->FrameRate = FString::Printf(TEXT("%.2f fps"), Format.GetFrameRateDecimal());
			Item->BitDepth = FString::FromInt(Format.GetBitDepthInt());

			switch (Format.ColorFormat)
			{
			case ERship2110ColorFormat::YCbCr_422:
				Item->ColorFormat = TEXT("YCbCr 4:2:2");
				break;
			case ERship2110ColorFormat::YCbCr_444:
				Item->ColorFormat = TEXT("YCbCr 4:4:4");
				break;
			case ERship2110ColorFormat::RGB_444:
				Item->ColorFormat = TEXT("RGB 4:4:4");
				break;
			case ERship2110ColorFormat::RGBA_4444:
				Item->ColorFormat = TEXT("RGBA 4:4:4:4");
				break;
			default:
				Item->ColorFormat = TEXT("Unknown");
				break;
			}

			switch (Sender->GetCaptureSource())
			{
			case ERship2110CaptureSource::RenderTarget:
				Item->CaptureSource = TEXT("RenderTarget");
				break;
			case ERship2110CaptureSource::Viewport:
				Item->CaptureSource = TEXT("Viewport");
				break;
			case ERship2110CaptureSource::SceneCapture:
				Item->CaptureSource = TEXT("SceneCapture");
				break;
			case ERship2110CaptureSource::External:
				Item->CaptureSource = TEXT("External");
				break;
			default:
				Item->CaptureSource = TEXT("Unknown");
				break;
			}

			const FRship2110TransportParams Transport = Sender->GetTransportParams();
			Item->Destination = FString::Printf(TEXT("%s:%d"), *Transport.DestinationIP, Transport.DestinationPort);

			const FRship2110StreamStats Stats = Sender->GetStatistics();
			Item->FramesSent = Stats.FramesSent;
			Item->FramesDropped = Stats.FramesDropped;
			Item->LateFrames = Stats.LateFrames;
			Item->BitrateMbps = Sender->GetBitrateMbps();
		}
#endif

		FString BoundContextId;
		FIntRect BoundRect;
		bool bHasBoundRect = false;
		if (Subsystem->GetBoundRenderContextBinding(StreamId, BoundContextId, BoundRect, bHasBoundRect))
		{
			Item->BoundContextId = BoundContextId;
			Item->bHasCaptureRect = bHasBoundRect && BoundRect.Area() > 0;
			Item->BoundCaptureRect = BoundRect;
			if (Item->bHasCaptureRect)
			{
				const int32 BoundWidth = BoundRect.Max.X - BoundRect.Min.X;
				const int32 BoundHeight = BoundRect.Max.Y - BoundRect.Min.Y;
				Item->BoundCaptureText = FString::Printf(TEXT("x=%d y=%d %dx%d"), BoundRect.Min.X, BoundRect.Min.Y, BoundWidth, BoundHeight);
			}
			else
			{
				Item->BoundCaptureText = TEXT("full");
			}
		}

		if (!Item->BoundContextId.IsEmpty())
		{
			BoundContextCounts.FindOrAdd(Item->BoundContextId, 0);
			BoundContextCounts[Item->BoundContextId] += 1;
		}

		StreamItems.Add(Item);
	}

	if (StreamListView.IsValid())
	{
		StreamListView->RequestListRefresh();
	}

	// Keep stream selection stable between refreshes
	if (SelectedStream.IsValid())
	{
		const FString PreviousStreamId = SelectedStream->StreamId;
		SelectedStream.Reset();
		for (const TSharedPtr<FRship2110MappingStreamItem>& Item : StreamItems)
		{
			if (Item->StreamId == PreviousStreamId)
			{
				SelectedStream = Item;
				if (StreamListView.IsValid())
				{
					StreamListView->SetItemSelection(Item, true);
				}
				break;
		}
	}
}

void SRship2110MappingPanel::RefreshContexts()
{
	ContextItems.Empty();

	if (!IsContentMappingAvailable())
	{
		if (ContextListView.IsValid())
		{
			ContextListView->RequestListRefresh();
		}
		return;
	}

	URshipContentMappingManager* MappingManager = GetContentMappingManager();
	if (!MappingManager)
	{
		if (ContextListView.IsValid())
		{
			ContextListView->RequestListRefresh();
		}
		return;
	}

	const TArray<FRshipRenderContextState> ContextStates = MappingManager->GetRenderContexts();
	for (const FRshipRenderContextState& Context : ContextStates)
	{
		TSharedPtr<FRship2110RenderContextItem> Item = MakeShared<FRship2110RenderContextItem>();
		Item->ContextId = Context.Id;
		Item->Name = Context.Name;
		Item->SourceType = Context.SourceType;
		Item->Resolution = FString::Printf(TEXT("%dx%d"), Context.Width, Context.Height);
		Item->Width = Context.Width;
		Item->Height = Context.Height;
		Item->CameraId = Context.CameraId;
		Item->bEnabled = Context.bEnabled;
		Item->bHasRenderTarget = Cast<UTextureRenderTarget2D>(Context.ResolvedTexture) != nullptr;
		Item->LastError = Context.LastError;
		Item->BoundStreamCount = BoundContextCounts.FindRef(Context.Id);
		Item->bBound = Item->BoundStreamCount > 0;
		ContextItems.Add(Item);
	}

	if (ContextListView.IsValid())
	{
		ContextListView->RequestListRefresh();
	}

	if (SelectedContext.IsValid())
	{
		const FString PreviousContextId = SelectedContext->ContextId;
		SelectedContext.Reset();
		for (const TSharedPtr<FRship2110RenderContextItem>& Item : ContextItems)
		{
			if (Item->ContextId == PreviousContextId)
			{
				SelectedContext = Item;
				if (ContextListView.IsValid())
				{
					ContextListView->SetItemSelection(Item, true);
				}
				break;
			}
		}
	}
}

void SRship2110MappingPanel::ReconcileSelection()
{
	if (!SelectedStream.IsValid() && StreamItems.Num() > 0)
	{
		SelectedStream = StreamItems[0];
		if (StreamListView.IsValid())
		{
			StreamListView->SetItemSelection(SelectedStream, true);
		}
	}
	if (!SelectedContext.IsValid() && ContextItems.Num() > 0)
	{
		SelectedContext = ContextItems[0];
		if (ContextListView.IsValid())
		{
			ContextListView->SetItemSelection(SelectedContext, true);
		}
	}
}

void SRship2110MappingPanel::UpdateSummaries()
{
	if (StreamSummaryText.IsValid())
	{
		StreamSummaryText->SetText(FText::Format(
			LOCTEXT("StreamSummaryFmt", "Active 2110 Streams: {0}"),
			FText::AsNumber(StreamItems.Num())));
	}

	if (ContextSummaryText.IsValid())
	{
		ContextSummaryText->SetText(FText::Format(
			LOCTEXT("ContextSummaryFmt", "Content Contexts: {0}"),
			FText::AsNumber(ContextItems.Num())));
	}

	if (BindingSummaryText.IsValid())
	{
		int32 BoundStreams = 0;
		for (const TSharedPtr<FRship2110MappingStreamItem>& Item : StreamItems)
		{
			if (!Item->BoundContextId.IsEmpty())
			{
				++BoundStreams;
			}
		}
		BindingSummaryText->SetText(FText::Format(
			LOCTEXT("BindingSummaryFmt", "Streams with bindings: {0}"),
			FText::AsNumber(BoundStreams)));
	}
}

void SRship2110MappingPanel::UpdateSelectionDetails()
{
	if (SelectedStream.IsValid())
	{
		SelectedStreamText->SetText(FText::FromString(SelectedStream->StreamId));

		SelectedStreamFormatText->SetText(FText::Format(
			LOCTEXT("StreamFormatFmt", "{0} | {1} | {2} | {3}"),
			FText::FromString(SelectedStream->Resolution),
			FText::FromString(SelectedStream->FrameRate),
			FText::FromString(SelectedStream->ColorFormat),
			FText::FromString(SelectedStream->BitDepth)));

		SelectedStreamStatsText->SetText(FText::Format(
			LOCTEXT("StreamStatsFmt", "Frames sent: {0}  | Dropped: {1}  | Late: {2}  | Bitrate: {3} Mbps"),
			FText::AsNumber(SelectedStream->FramesSent),
			FText::AsNumber(SelectedStream->FramesDropped),
			FText::AsNumber(SelectedStream->LateFrames),
			FText::AsNumber(SelectedStream->BitrateMbps, &FNumberFormattingOptions::DefaultWithGrouping().SetMaximumFractionalDigits(2))));

		if (!SelectedStream->BoundContextId.IsEmpty())
		{
			if (SelectedStream->bHasCaptureRect)
			{
				SelectedStreamBindingText->SetText(FText::Format(
					LOCTEXT("StreamBoundWithCaptureFmt", "Bound to context: {0} ({1})"),
					FText::FromString(SelectedStream->BoundContextId),
					FText::FromString(SelectedStream->BoundCaptureText)));
			}
			else
			{
				SelectedStreamBindingText->SetText(FText::Format(
					LOCTEXT("StreamBoundFmt", "Bound to context: {0}"),
					FText::FromString(SelectedStream->BoundContextId)));
			}
			SelectedStreamBindingText->SetColorAndOpacity(FLinearColor::Green);
		}
		else
		{
			SelectedStreamBindingText->SetText(LOCTEXT("StreamUnbound", "No context bound"));
			SelectedStreamBindingText->SetColorAndOpacity(FLinearColor::Yellow);
		}
	}
	else
	{
		SelectedStreamText->SetText(LOCTEXT("NoStream", "No stream selected"));
		SelectedStreamFormatText->SetText(FText::GetEmpty());
		SelectedStreamStatsText->SetText(FText::GetEmpty());
		SelectedStreamBindingText->SetText(FText::GetEmpty());
	}

	if (SelectedContext.IsValid())
	{
		FString DisplayContext = SelectedContext->Name.IsEmpty() ? SelectedContext->ContextId : SelectedContext->Name;
		SelectedContextText->SetText(FText::FromString(DisplayContext));

		FText BoundText = SelectedContext->bBound
			? FText::Format(LOCTEXT("ContextBoundCount", "Bound by {0} stream(s)"), FText::AsNumber(SelectedContext->BoundStreamCount))
			: LOCTEXT("ContextUnboundCount", "Not bound");

		SelectedContextDetailsText->SetText(FText::Format(
			LOCTEXT("ContextDetailsFmt", "Type: {0}  | Resolution: {1}  | RT Ready: {2}  | {3}"),
			FText::FromString(SelectedContext->SourceType),
			FText::FromString(SelectedContext->Resolution),
			SelectedContext->bHasRenderTarget ? LOCTEXT("ContextRtYes", "Yes") : LOCTEXT("ContextRtNo", "No"),
			BoundText));
	}
	else
	{
		SelectedContextText->SetText(LOCTEXT("NoContext", "No context selected"));
		SelectedContextDetailsText->SetText(FText::GetEmpty());
	}

	if (BindingStatusText.IsValid())
	{
		if (!Is2110RuntimeAvailable())
		{
			BindingStatusText->SetText(LOCTEXT("BindingRuntimeMissing", "Enable Rship2110 runtime before binding."));
			BindingStatusText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else if (CanStart() || CanStop() || CanUnbind() || CanBind())
		{
			BindingStatusText->SetText(LOCTEXT("BindingReady", "Select stream + context, then bind or control stream."));
			BindingStatusText->SetColorAndOpacity(FLinearColor::Green);
		}
		else if (SelectedStream.IsValid())
		{
			BindingStatusText->SetText(LOCTEXT("BindingNoContext", "Pick a render context to bind."));
			BindingStatusText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			BindingStatusText->SetText(LOCTEXT("BindingNoStream", "Select a stream from the stream list."));
			BindingStatusText->SetColorAndOpacity(FLinearColor::Yellow);
		}
	}
}

void SRship2110MappingPanel::UpdateBindingInputsFromSelection()
{
	if (!CaptureXText.IsValid() || !CaptureYText.IsValid() || !CaptureWText.IsValid() || !CaptureHText.IsValid())
	{
		return;
	}

	if (!SelectedContext.IsValid())
	{
		CaptureXText->SetText(FText::GetEmpty());
		CaptureYText->SetText(FText::GetEmpty());
		CaptureWText->SetText(FText::GetEmpty());
		CaptureHText->SetText(FText::GetEmpty());
		return;
	}

	const bool bHasMatchingBoundCapture = SelectedStream.IsValid() && SelectedStream->BoundContextId == SelectedContext->ContextId && SelectedStream->bHasCaptureRect;
	if (bHasMatchingBoundCapture)
	{
		CaptureXText->SetText(FText::AsNumber(SelectedStream->BoundCaptureRect.Min.X));
		CaptureYText->SetText(FText::AsNumber(SelectedStream->BoundCaptureRect.Min.Y));
		CaptureWText->SetText(FText::AsNumber(SelectedStream->BoundCaptureRect.Max.X - SelectedStream->BoundCaptureRect.Min.X));
		CaptureHText->SetText(FText::AsNumber(SelectedStream->BoundCaptureRect.Max.Y - SelectedStream->BoundCaptureRect.Min.Y));
	}
	else
	{
		CaptureXText->SetText(FText::GetEmpty());
		CaptureYText->SetText(FText::GetEmpty());
		CaptureWText->SetText(FText::GetEmpty());
		CaptureHText->SetText(FText::GetEmpty());
	}
}

bool SRship2110MappingPanel::ParseCropField(const TSharedPtr<SEditableTextBox>& TextBox, int32& OutValue) const
{
	if (!TextBox.IsValid())
	{
		return false;
	}

	const FString Text = TextBox->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return false;
	}

	return FDefaultValueHelper::ParseInt(Text, OutValue);
}

bool SRship2110MappingPanel::GetBindCaptureRect(FIntRect& OutRect) const
{
	int32 X = 0;
	int32 Y = 0;
	int32 Width = 0;
	int32 Height = 0;

	if (!ParseCropField(CaptureXText, X) || !ParseCropField(CaptureYText, Y) || !ParseCropField(CaptureWText, Width) || !ParseCropField(CaptureHText, Height))
	{
		return false;
	}

	if (Width <= 0 || Height <= 0)
	{
		return false;
	}

	OutRect = FIntRect(X, Y, X + Width, Y + Height);
	return true;
}

void SRship2110MappingPanel::OnRefreshClicked()
{
	RefreshPanel();
}

FReply SRship2110MappingPanel::OnBindClicked()
{
	if (!CanBind())
	{
		return FReply::Handled();
	}

#if RSHIP_EDITOR_HAS_2110
	if (URship2110Subsystem* Subsystem = Get2110Subsystem())
	{
		FIntRect CropRect;
		const bool bUseCrop = GetBindCaptureRect(CropRect) && SelectedContext.IsValid() && SelectedContext->Width > 0 && SelectedContext->Height > 0;
		const int32 SourceWidth = SelectedContext.IsValid() ? SelectedContext->Width : 0;
		const int32 SourceHeight = SelectedContext.IsValid() ? SelectedContext->Height : 0;
		int32 FinalCaptureX = CropRect.Min.X;
		int32 FinalCaptureY = CropRect.Min.Y;
		int32 FinalCaptureWidth = CropRect.Max.X - CropRect.Min.X;
		int32 FinalCaptureHeight = CropRect.Max.Y - CropRect.Min.Y;

		bool bBound = false;
		if (bUseCrop)
		{
			const int32 RequestedWidth = CropRect.Max.X - CropRect.Min.X;
			const int32 RequestedHeight = CropRect.Max.Y - CropRect.Min.Y;
			const int32 ClampedX = FMath::Clamp(CropRect.Min.X, 0, FMath::Max(0, SourceWidth - 1));
			const int32 ClampedY = FMath::Clamp(CropRect.Min.Y, 0, FMath::Max(0, SourceHeight - 1));
			const int32 ClampedWidth = FMath::Clamp(RequestedWidth, 1, SourceWidth - ClampedX);
			const int32 ClampedHeight = FMath::Clamp(RequestedHeight, 1, SourceHeight - ClampedY);

			FinalCaptureX = ClampedX;
			FinalCaptureY = ClampedY;
			FinalCaptureWidth = ClampedWidth;
			FinalCaptureHeight = ClampedHeight;

			bBound = Subsystem->BindVideoStreamToRenderContextWithRect(
				SelectedStream->StreamId,
				SelectedContext->ContextId,
				FIntRect(ClampedX, ClampedY, ClampedX + ClampedWidth, ClampedY + ClampedHeight));
		}
		else
		{
			bBound = Subsystem->BindVideoStreamToRenderContext(SelectedStream->StreamId, SelectedContext->ContextId);
		}

		if (bBound)
		{
			if (BindingStatusText.IsValid())
			{
				if (bUseCrop)
				{
					BindingStatusText->SetText(FText::Format(
						LOCTEXT("BindOkWithCapture", "Bound {0} -> {1} (x={2}, y={3}, {4}x{5})"),
						FText::FromString(SelectedStream->StreamId),
						FText::FromString(SelectedContext->ContextId),
						FText::AsNumber(FinalCaptureX),
						FText::AsNumber(FinalCaptureY),
						FText::AsNumber(FinalCaptureWidth),
						FText::AsNumber(FinalCaptureHeight)));
				}
				else
				{
					BindingStatusText->SetText(FText::Format(
						LOCTEXT("BindOk", "Bound {0} -> {1}"),
						FText::FromString(SelectedStream->StreamId),
						FText::FromString(SelectedContext->ContextId)));
				}
				BindingStatusText->SetColorAndOpacity(FLinearColor::Green);
			}
		}
		else if (BindingStatusText.IsValid())
		{
			BindingStatusText->SetText(FText::Format(
				LOCTEXT("BindFailed", "Failed to bind {0}. Check context render target availability."),
				FText::FromString(SelectedStream->StreamId)));
			BindingStatusText->SetColorAndOpacity(FLinearColor::Red);
		}
	}
#else
	if (BindingStatusText.IsValid())
	{
		BindingStatusText->SetText(LOCTEXT("RuntimeMissing", "Rship2110 runtime not available."));
		BindingStatusText->SetColorAndOpacity(FLinearColor::Red);
	}
#endif

	RefreshPanel();
	return FReply::Handled();
}

FReply SRship2110MappingPanel::OnUnbindClicked()
{
	if (!CanUnbind())
	{
		return FReply::Handled();
	}

#if RSHIP_EDITOR_HAS_2110
	if (URship2110Subsystem* Subsystem = Get2110Subsystem())
	{
		if (Subsystem->UnbindVideoStreamFromRenderContext(SelectedStream->StreamId))
		{
			if (BindingStatusText.IsValid())
			{
				BindingStatusText->SetText(FText::Format(LOCTEXT("UnbindOk", "Unbound stream {0}"), FText::FromString(SelectedStream->StreamId)));
				BindingStatusText->SetColorAndOpacity(FLinearColor::Green);
			}
		}
	}
#endif

	RefreshPanel();
	return FReply::Handled();
}

FReply SRship2110MappingPanel::OnStartStreamClicked()
{
	if (!CanStart())
	{
		return FReply::Handled();
	}

#if RSHIP_EDITOR_HAS_2110
	if (URship2110Subsystem* Subsystem = Get2110Subsystem())
	{
		Subsystem->StartStream(SelectedStream->StreamId);
	}
#endif

	RefreshPanel();
	return FReply::Handled();
}

FReply SRship2110MappingPanel::OnStopStreamClicked()
{
	if (!CanStop())
	{
		return FReply::Handled();
	}

#if RSHIP_EDITOR_HAS_2110
	if (URship2110Subsystem* Subsystem = Get2110Subsystem())
	{
		Subsystem->StopStream(SelectedStream->StreamId);
	}
#endif

	RefreshPanel();
	return FReply::Handled();
}

FReply SRship2110MappingPanel::OnResetStatsClicked()
{
	if (!SelectedStream.IsValid())
	{
		return FReply::Handled();
	}

#if RSHIP_EDITOR_HAS_2110
	if (URship2110Subsystem* Subsystem = Get2110Subsystem())
	{
		URship2110VideoSender* Sender = Subsystem->GetVideoSender(SelectedStream->StreamId);
		if (Sender)
		{
			Sender->ResetStatistics();
		}
	}
#endif

	RefreshPanel();
	return FReply::Handled();
}

bool SRship2110MappingPanel::CanBind() const
{
	return Is2110RuntimeAvailable() && IsContentMappingAvailable() && SelectedStream.IsValid() && SelectedContext.IsValid() && !SelectedStream->bStreamMissing;
}

bool SRship2110MappingPanel::CanUnbind() const
{
	return CanBind() && !SelectedStream->BoundContextId.IsEmpty();
}

bool SRship2110MappingPanel::CanStart() const
{
	return SelectedStream.IsValid() && !SelectedStream->bIsRunning && !SelectedStream->bStreamMissing;
}

bool SRship2110MappingPanel::CanStop() const
{
	return SelectedStream.IsValid() && SelectedStream->bIsRunning && !SelectedStream->bStreamMissing;
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildOverviewSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "SMPTE 2110 Mapping"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshButton", "Refresh"))
				.OnClicked(this, &SRship2110MappingPanel::OnRefreshClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(ModuleStatusText, STextBlock)
					.Text(LOCTEXT("ModuleUnknown", "SMPTE 2110 runtime: checking..."))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SAssignNew(ContentMappingStatusText, STextBlock)
					.Text(LOCTEXT("MappingUnknown", "Content Mapping: checking..."))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(StreamSummaryText, STextBlock)
						.Text(LOCTEXT("StreamSummaryInit", "Active 2110 Streams: 0"))
					]
					+ SHorizontalBox::Slot()
					.Padding(12.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(ContextSummaryText, STextBlock)
						.Text(LOCTEXT("ContextSummaryInit", "Content Contexts: 0"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(BindingSummaryText, STextBlock)
						.Text(LOCTEXT("BindingSummaryInit", "Streams with bindings: 0"))
					]
				]
			]
		];
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildStreamListSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("StreamListHeader", "Active 2110 Streams"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.16f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StreamStateColumn", "State"))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.34f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("StreamIdColumn", "Stream ID"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("StreamFormatColumn", "Format"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("StreamBindColumn", "Bound Context"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.10f)
					[
						SNew(STextBlock).Text(LOCTEXT("StreamRateColumn", "Mb/s"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(StreamListView, SListView<TSharedPtr<FRship2110MappingStreamItem>>)
					.ListItemsSource(&StreamItems)
					.OnGenerateRow(this, &SRship2110MappingPanel::OnGenerateStreamRow)
					.OnSelectionChanged(this, &SRship2110MappingPanel::OnStreamSelectionChanged)
					.SelectionMode(ESelectionMode::Single)
				]
			]
		];
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildContextListSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ContextListHeader", "Content Mapping Render Contexts"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(4.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.26f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ContextNameColumn", "Context"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.12f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ContextTypeColumn", "Type"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.16f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ContextResColumn", "Resolution"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.20f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ContextRTColumn", "Render Target"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
					+ SHorizontalBox::Slot().FillWidth(0.16f).Padding(0.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ContextBoundColumn", "Bindings"))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(ContextListView, SListView<TSharedPtr<FRship2110RenderContextItem>>)
					.ListItemsSource(&ContextItems)
					.OnGenerateRow(this, &SRship2110MappingPanel::OnGenerateContextRow)
					.OnSelectionChanged(this, &SRship2110MappingPanel::OnContextSelectionChanged)
					.SelectionMode(ESelectionMode::Single)
				]
			]
		];
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildBindingSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BindingHeader", "Binding Controls"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BindSelectedStreamLabel", "Stream:"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(SelectedStreamText, STextBlock)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BindSelectedContextLabel", "Context:"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						SAssignNew(SelectedContextText, STextBlock)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CaptureRectHelp", "Capture Region (pixels, optional): x, y, width, height. Leave blank for full context."))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("CaptureXLabel", "X:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SAssignNew(CaptureXText, SEditableTextBox)
						.HintText(LOCTEXT("CaptureXHint", "x"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("CaptureYLabel", "Y:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SAssignNew(CaptureYText, SEditableTextBox)
						.HintText(LOCTEXT("CaptureYHint", "y"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("CaptureWLabel", "W:"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SAssignNew(CaptureWText, SEditableTextBox)
						.HintText(LOCTEXT("CaptureWHint", "width"))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("CaptureHLabel", "H:"))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SAssignNew(CaptureHText, SEditableTextBox)
						.HintText(LOCTEXT("CaptureHHint", "height"))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("BindButton", "Bind Stream -> Context"))
						.OnClicked(this, &SRship2110MappingPanel::OnBindClicked)
						.IsEnabled_Lambda([this]() { return CanBind(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("UnbindButton", "Unbind"))
						.OnClicked(this, &SRship2110MappingPanel::OnUnbindClicked)
						.IsEnabled_Lambda([this]() { return CanUnbind(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("StartButton", "Start"))
						.OnClicked(this, &SRship2110MappingPanel::OnStartStreamClicked)
						.IsEnabled_Lambda([this]() { return CanStart(); })
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("StopButton", "Stop"))
						.OnClicked(this, &SRship2110MappingPanel::OnStopStreamClicked)
						.IsEnabled_Lambda([this]() { return CanStop(); })
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ResetStatsButton", "Reset Stats"))
						.OnClicked(this, &SRship2110MappingPanel::OnResetStatsClicked)
						.IsEnabled_Lambda([this]() { return SelectedStream.IsValid(); })
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SAssignNew(BindingStatusText, STextBlock)
					.Text(LOCTEXT("BindingStatusInit", "Select stream and context to create a binding."))
				]
			]
		];
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildSelectionDetailsSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("DetailsHeader", "Selection Details"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(8.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(SelectedStreamFormatText, STextBlock)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(SelectedStreamStatsText, STextBlock)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(SelectedStreamBindingText, STextBlock)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
				[
					SAssignNew(SelectedContextDetailsText, STextBlock)
				]
			]
		];
}

TSharedRef<SWidget> SRship2110MappingPanel::BuildUserGuideSection()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GuideHeader", "Usage"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.WrapTextAt(980.0f)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			.Text(LOCTEXT("GuideText",
				"Bind a stream to a render context and run it live in the editor."
				" The sender consumes the render context's resolved render target each frame and streams it over 2110."
				" Keep stream format resolution/frame-rate aligned with the bound context output size."
				" You can capture a cropped output by entering x,y,width,height; leave all empty for full context."))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.WrapTextAt(980.0f)
			.ColorAndOpacity(FLinearColor(0.95f, 0.8f, 0.35f, 1.0f))
			.Text(LOCTEXT("GuideTextWarning",
				"Hint: use the same render-context boundaries used by nDisplay/content mapping upstream."
				" This keeps upstream distribution and frame-boundary calculations intact while still using crop for partial output."))
		];
}

TSharedRef<ITableRow> SRship2110MappingPanel::OnGenerateStreamRow(TSharedPtr<FRship2110MappingStreamItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FRship2110MappingStreamItem>>, OwnerTable);
	}

	return SNew(STableRow<TSharedPtr<FRship2110MappingStreamItem>>, OwnerTable)
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.16f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
					.BorderBackgroundColor(Item->StateColor)
					.Padding(FMargin(6.0f, 2.0f))
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->StateText))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.34f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->StreamId))
			]
			+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("%s | %s | %s-bit"),
					*Item->Resolution,
					*Item->FrameRate,
					*Item->BitDepth)))
			]
			+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->BoundContextId.IsEmpty() ? TEXT("unbound") : Item->BoundContextId))
				.ColorAndOpacity(Item->BoundContextId.IsEmpty() ? FLinearColor::Yellow : FLinearColor::Green)
			]
			+ SHorizontalBox::Slot().FillWidth(0.10f)
			[
				SNew(STextBlock)
				.Text(FText::AsNumber(Item->BitrateMbps, &FNumberFormattingOptions::DefaultWithGrouping().SetMaximumFractionalDigits(2)))
			]
		];
}

TSharedRef<ITableRow> SRship2110MappingPanel::OnGenerateContextRow(TSharedPtr<FRship2110RenderContextItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (!Item.IsValid())
	{
		return SNew(STableRow<TSharedPtr<FRship2110RenderContextItem>>, OwnerTable);
	}

	const FLinearColor RowBg = Item->bBound ? FLinearColor(0.0f, 0.4f, 0.0f, 0.14f) : FLinearColor::Transparent;

	return SNew(STableRow<TSharedPtr<FRship2110RenderContextItem>>, OwnerTable)
		.Padding(2.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(RowBg)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.26f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Name.IsEmpty() ? Item->ContextId : Item->Name))
				]
				+ SHorizontalBox::Slot().FillWidth(0.12f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->SourceType))
				]
				+ SHorizontalBox::Slot().FillWidth(0.16f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->Resolution))
				]
				+ SHorizontalBox::Slot().FillWidth(0.20f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Item->bHasRenderTarget ? LOCTEXT("HasRenderTarget", "Ready") : LOCTEXT("NoRenderTarget", "Missing"))
					.ColorAndOpacity(Item->bHasRenderTarget ? FLinearColor::Green : FLinearColor::Red)
				]
				+ SHorizontalBox::Slot().FillWidth(0.16f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("BoundCountFmt", "{0}"), FText::AsNumber(Item->BoundStreamCount)))
					.ColorAndOpacity(Item->bBound ? FLinearColor::Green : FSlateColor::UseSubduedForeground().GetSpecifiedColor())
				]
			]
		];
}

void SRship2110MappingPanel::OnStreamSelectionChanged(TSharedPtr<FRship2110MappingStreamItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedStream = Item;
	UpdateSelectionDetails();
	UpdateBindingInputsFromSelection();
}

void SRship2110MappingPanel::OnContextSelectionChanged(TSharedPtr<FRship2110RenderContextItem> Item, ESelectInfo::Type SelectInfo)
{
	SelectedContext = Item;
	UpdateSelectionDetails();
	UpdateBindingInputsFromSelection();
}

#undef LOCTEXT_NAMESPACE
