// Fill out your copyright notice in the Description page of Project Settings.

#include "Action.h"
#include "GameFramework/Actor.h"
#include "Misc/OutputDevice.h"
#include "FJsonSchema.h"
#include "Util.h"

Action::Action(FString id,  UFunction *function)
{
    this->id = id;
    this->props = new TDoubleLinkedList<RshipActionProperty>();
    this->functionName = function->GetName();
    this->parents = TSet<AActor*>();
    this->schema = MakeShareable(new FJsonSchema());
    this->UpdateSchema(function);
}

Action::~Action()
{
}

FString Action::GetId()
{
    return this->id;
}

void Action::Take(TSharedPtr<FJsonObject> data)
{
    UE_LOG(LogTemp, Warning, TEXT("Taking Action %s"), *this->id);

    // use our props list to build a string of our arguments

    FString* args = new FString();

    args->Append(this->functionName);

    for (const auto prop : *this->props)
    {
        auto piece = data->TryGetField(prop.Name)->AsString();
        args->Append(" \"");
        args->Append(piece);
        args->Append("\"");
	}

    data.Reset();

    // get current output device

    FOutputDeviceNull out = FOutputDeviceNull();


    TCHAR *outStr = args->GetCharArray().GetData();

    UE_LOG(LogTemp, Warning, TEXT("Calling function with args: %s"), outStr);

    TSet<AActor*>* safeParents = new TSet<AActor*>;

    for (auto parent : this->parents)
    {
        if (parent->IsValidLowLevelFast()) {
            safeParents->Add(parent);
        }
	}

    

    this->parents = *safeParents;


    for (auto parent : this->parents)
    {
        if (parent) {
            try {
				parent->CallFunctionByNameWithArguments(outStr, out, NULL, true);
			}
            catch (const std::exception& e) {
                UE_LOG(LogTemp, Warning, TEXT("Caught exception: %s"), e.what());
            }
        }
	}

    delete args;
}

void Action::AddParent(AActor* parent)
{
    this->parents.Add(parent);
}

void Action::UpdateSchema(UFunction* handler) {

    this->props->Empty();
    this->schema->Empty();
    //   11001010
    //   00001111
    //   00001010

    for (TFieldIterator<FProperty> It(handler); It && (It->PropertyFlags & (CPF_Parm)) == CPF_Parm; ++It)
    {
        FProperty* Property = *It;
        FString PropertyName = Property->GetName();
        FName PropertyType = Property->GetClass()->GetFName();


        RshipActionProperty prop = RshipActionProperty({ PropertyName, PropertyType.ToString() });

        UE_LOG(LogTemp, Warning, TEXT("Property: %s, Type: %s"), *PropertyName, *PropertyType.ToString());

        /*

        [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Boolean, Type: BoolProperty
        [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Byte, Type: ByteProperty
        [2024-03-20T18:20:37.718Z]LogTemp: Warning: Property: Integer, Type: IntProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Integer64, Type: Int64Property
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Float, Type: DoubleProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Name, Type: NameProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: String, Type: StrProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Text, Type: TextProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Vector, Type: StructProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Rotator, Type: StructProperty
        [2024-03-20T18:20:37.719Z]LogTemp: Warning: Property: Transform, Type: StructProperty

        */

        if (PropertyType == "BoolProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::Boolean());
        }
        else if (PropertyType == "ByteProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::Number());
        }
        else if (PropertyType == "IntProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::Number());
        }
        else if (PropertyType == "Int64Property") {
            this->schema->Prop(PropertyName, FJsonSchema::Number());
        }
        else if (PropertyType == "DoubleProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::Number());
        }
        else if (PropertyType == "NameProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::String());
        }
        else if (PropertyType == "StrProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::String());
        }
        else if (PropertyType == "TextProperty")
        {
            this->schema->Prop(PropertyName, FJsonSchema::String());
        }
        else if (PropertyType == "StructProperty")
        {
            // handle vector, rotator, transform here
            //this->schema->Prop(PropertyName, FJsonSchema::Object());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Property type not supported: %s"), *PropertyType.ToString());
        }

        this->props->AddTail(prop);
    }

}

TSharedPtr<FJsonObject> Action::GetSchema()
{
    return this->schema->ValueOf();
}
