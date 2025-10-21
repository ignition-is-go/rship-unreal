
using namespace std;

TSharedPtr<FJsonObject> MakeSet(FString itemType, TSharedPtr<FJsonObject> data);

FString GetUniqueMachineId();

TSharedPtr<FJsonObject> WrapWSEvent(TSharedPtr<FJsonObject> payload);

