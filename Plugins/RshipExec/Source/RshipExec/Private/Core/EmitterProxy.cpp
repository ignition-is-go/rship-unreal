#include "Core/EmitterProxy.h"

#include "SchemaHelpers.h"

FRshipEmitterProxy FRshipEmitterProxy::FromDelegateProperty(const FString& InId, const FString& InName, FMulticastInlineDelegateProperty* InEmitter)
{
	FRshipEmitterProxy Proxy;
	Proxy.Id = InId;
	Proxy.Name = InName;
	if (InEmitter && InEmitter->SignatureFunction)
	{
		BuildSchemaPropsFromUFunction(InEmitter->SignatureFunction, *Proxy.Props);
	}
	return Proxy;
}

TSharedPtr<FJsonObject> FRshipEmitterProxy::GetSchema() const
{
	return Props.IsValid() ? PropsToSchema(Props.Get()) : nullptr;
}
