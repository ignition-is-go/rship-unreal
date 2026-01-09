#include "SchemaHelpers.h"
#include "Logs.h"

static void BuildChildrenForStruct(const UScriptStruct *InStruct, SchemaNode &OutProp)
{
    for (TFieldIterator<FProperty> It(InStruct); It; ++It)
    {
        const FProperty *Field = *It;
    SchemaNode Child;
        Child.Name = Field->GetName();
        Child.Type = Field->GetClass()->GetFName().ToString();
        if (const FStructProperty *NestedStruct = CastField<FStructProperty>(Field))
        {
            if (const UScriptStruct *NestedScript = NestedStruct->Struct)
            {
                BuildChildrenForStruct(NestedScript, Child);
            }
        }
        OutProp.Children.Add(Child);
    }
}

void BuildSchemaPropsFromUFunction(UFunction* Handler, TDoubleLinkedList<SchemaNode>& OutProps)
{
    OutProps.Empty();

    for (TFieldIterator<FProperty> It(Handler); It && (It->PropertyFlags & (CPF_Parm)) == CPF_Parm; ++It)
    {
        FProperty *Property = *It;
        SchemaNode Prop;
		ConstructSchemaProp(Property, Prop);
        OutProps.AddTail(Prop);
    }
}

void BuildSchemaPropsFromFProperty(FProperty* Property, TDoubleLinkedList<SchemaNode>& OutProps)
{
    OutProps.Empty();
	SchemaNode Prop;
    ConstructSchemaProp(Property, Prop);
	OutProps.AddTail(Prop);
}

void ConstructSchemaProp(FProperty* Property, SchemaNode& OutProp)
{
    OutProp.Name = Property->GetName();
    OutProp.Type = Property->GetClass()->GetFName().ToString();

    if (const FStructProperty *StructProp = CastField<FStructProperty>(Property))
    {
        if (const UScriptStruct *ScriptStruct = StructProp->Struct)
        {
            BuildChildrenForStruct(ScriptStruct, OutProp);
        }
    }

    UE_LOG(LogRshipExec, Verbose, TEXT("Constructed SchemaNode - %s: %s"), *OutProp.Name, *OutProp.Type);
}



static bool IsStringLike(const FString &UnrealType)
{
    return UnrealType == TEXT("StrProperty") || UnrealType == TEXT("TextProperty") || UnrealType == TEXT("NameProperty");
}

static FString FormatValueForUnrealArg(const SchemaNode &SchemaProp, const TSharedPtr<FJsonValue> &JsonVal, bool bQuoteStrings);

static FString FormatStructForUnrealArg(const SchemaNode &SchemaProp, const TSharedPtr<FJsonObject> &Obj, bool bQuoteStrings)
{
    TArray<FString> Pairs;
    Pairs.Reserve(SchemaProp.Children.Num());
    for (const SchemaNode &Child : SchemaProp.Children)
    {
        const TSharedPtr<FJsonValue> ChildVal = Obj.IsValid() ? Obj->TryGetField(Child.Name) : nullptr;
        const FString ChildStr = FormatValueForUnrealArg(Child, ChildVal, bQuoteStrings);
        if (!ChildStr.IsEmpty())
        {
            Pairs.Add(FString::Printf(TEXT("%s=%s"), *Child.Name, *ChildStr));
        }
    }
    return FString::Printf(TEXT("(%s)"), *FString::Join(Pairs, TEXT(",")));
}

static FString FormatValueForUnrealArg(const SchemaNode &SchemaProp, const TSharedPtr<FJsonValue> &JsonVal, bool bQuoteStrings)
{
    if (SchemaProp.Type == TEXT("StructProperty"))
    {
        const TSharedPtr<FJsonObject> AsObj = (JsonVal.IsValid() && JsonVal->Type == EJson::Object) ? JsonVal->AsObject() : MakeShareable(new FJsonObject());
        return FormatStructForUnrealArg(SchemaProp, AsObj, bQuoteStrings);
    }

    if (!JsonVal.IsValid())
    {
        return FString("");
    }

    FString Out;
    switch (JsonVal->Type)
    {
    case EJson::String:
        Out = JsonVal->AsString();
        break;
    case EJson::Number:
        Out = FString::SanitizeFloat(JsonVal->AsNumber());
        break;
    case EJson::Boolean:
        Out = JsonVal->AsBool() ? TEXT("true") : TEXT("false");
        break;
    case EJson::Object:
        // Non-struct object fallback
        Out = GetJsonString(JsonVal->AsObject());
        break;
    case EJson::Array:
        // Array handling deferred per request
        Out = TEXT("[]");
        break;
    default:
        Out = FString("");
        break;
    }

    if (bQuoteStrings && IsStringLike(SchemaProp.Type))
    {
        Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
        return FString::Printf(TEXT("\"%s\""), *Out);
    }
    return Out;
}

FString BuildArgStringFromJson(const TDoubleLinkedList<SchemaNode>& Props, const TSharedRef<FJsonObject>& Data, bool bQuoteStrings)
{
    FString Args;
    for (const SchemaNode &Prop : Props)
    {
        const TSharedPtr<FJsonValue> Piece = Data->TryGetField(Prop.Name);
        if (!Piece.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("Missing field: %s"), *Prop.Name);
            continue;
        }
        if (!Args.IsEmpty())
        {
            Args.Append(TEXT(" "));
        }
        Args.Append(FormatValueForUnrealArg(Prop, Piece, bQuoteStrings));
    }
    return Args;
}
