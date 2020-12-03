
#define CURL_STATICLIB
#define SODIUM_STATIC
#define PHANTASMA_IMPLEMENTATION
#include "sodium.h"

#define PHANTASMA_EXCEPTION(x) { DebugBreak(); }
#define PHANTASMA_EXCEPTION_MESSAGE(literal, string) { DebugBreak(); }

#include "phantasma/Adapters/PhantasmaAPI_curl.h"
#include "phantasma/PhantasmaAPI.h"
#include "phantasma/Adapters/PhantasmaAPI_sodium.h"
#include "phantasma/Numerics/BigInteger.h"
#include "phantasma/Utils/RpcUtils.h"
#include "phantasma/VM/ScriptBuilder.h"

using namespace phantasma;


#include <inttypes.h>
#include <assert.h>
#define eiASSERT(x) assert(x)

#include "hash.h"
#include "random.h"

#include "goati_nft.h"


struct CrateInfo
{
	struct ItemGroup
	{
		PHANTASMA_VECTOR<int64_t> items;
	};
	struct ItemRoll
	{
		PHANTASMA_VECTOR<int64_t> groupPercentages;
	};
	int64_t itemId;

	PHANTASMA_VECTOR<ItemGroup> groups;
	PHANTASMA_VECTOR<ItemRoll> rolls;
};

#include "Audit.h"

int main( int argc, char *argv[] )
{
	const char* txHash = "";
	const char* nexus = "mainnet";
	const char* host = "https://pavillionhub.com/api";
	if( argc > 1 )
		txHash = argv[1];
	else
	{
		printf("usage: blindbag_audit txhash [host] [nexus]");
		return 1;
	}
	if( argc > 2 )
		host = argv[2];
	if( argc > 3 )
		nexus = argv[3];
	AuditResult result = AuditBoxTx(host, nexus, txHash);
	switch(result)
	{
	case Error:
		printf("The audit tool cannot handle this situation");
		return 3;
	case Unsuitable:
		printf("Unsuitable transaction");
		return 2;
	case Indeterminate:
		printf("Unable to validate at this time");
		return 1;
	case Mismatch:
		printf("Auditing found a mismatch!!! Uh oh!");
		return -1;
	case Match:
		printf("Auditing OK");
		return 0;
	}
	return 1;
}

