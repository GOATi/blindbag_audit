#pragma once

enum AuditResult
{
	Error,
	Unsuitable,
	Indeterminate,
	Mismatch,
	Match
};

static AuditResult AuditBoxTx(const char* szhost, const char* sznexus, const char* txHash)
{
	String host = String(szhost);
	String nexus = String(sznexus);

	CurlClient http(host);
	rpc::PhantasmaAPI api(http);

	rpc::PhantasmaError err;
	rpc::Transaction tx = api.GetTransaction(txHash, &err);

	PHANTASMA_VECTOR<BigInteger> nftIdsMinted;
	BigInteger nftId;
	int numBurnt = 0;
	for(const auto& evt : tx.events)
	{
		if(0==evt.kind.compare(PHANTASMA_LITERAL("TokenBurn")))
		{
			TokenEventData data = Serialization<TokenEventData>::Unserialize(Base16::Decode(evt.data));
			if (0!=data.symbol.compare(PHANTASMA_LITERAL("TTRS")))
				continue;
			nftId = data.value;
			++numBurnt;
		}
		else if(0==evt.kind.compare(PHANTASMA_LITERAL("TokenMint")))
		{
			TokenEventData data = Serialization<TokenEventData>::Unserialize(Base16::Decode(evt.data));
			if (0!=data.symbol.compare(PHANTASMA_LITERAL("TTRS")))
				continue;
			nftIdsMinted.push_back(data.value);
		}
	}
	if( nftId.IsZero() || nftIdsMinted.empty() )
		return Unsuitable;

	if( numBurnt > 1 )
		return Error;//TODO - need to handle transactions that opened multiple NFTs at once!

	auto data = api.GetTokenData(PHANTASMA_LITERAL("TTRS"), nftId.ToString().c_str(), &err);
	if( err.code != 0 )
		return Indeterminate;

	int64_t itemId;
	GOATiNftMintingSource mintingSource;
	int32_t sourceData;
	GOATiNftSeason season;
	int32_t timestamp;
	int64_t randomSeed;
	BigInteger counter;
	Address originalOwner;
	ByteArray crateSecret;
	GOATiNftType type;
	if( !GoatiNftRom_DecodeItem(Base16::Decode(data.rom), itemId, mintingSource, sourceData, 
		season, timestamp, randomSeed, counter, originalOwner,
		crateSecret, type) )
		return Unsuitable;
	if( type != GOATiNftType::Crate )
		return Unsuitable;
	if(crateSecret.size() != CrateSecret::LENGTH)
		return Error;

	ByteArray ram = Base16::Decode(data.ram);

	if( ram.size() < 8*5+Hash::Length)
		return Error;

	uint64_t ram_crateSeed[3] = {};
	uint64_t ram_randomSeed = 0;
	uint64_t ram_finalSeed = 0;
	Byte* ram_txSeed = 0;

	memcpy(&ram_crateSeed,  &ram[0], 8*3);
	memcpy(&ram_randomSeed, &ram[8*3], 8);
	memcpy(&ram_finalSeed,  &ram[8*4], 8);
	ram_txSeed = &ram[8*5];

	if( randomSeed != ram_randomSeed )
		return Mismatch;

	if( !ValidateCrateSecret(randomSeed, ram_crateSeed, &crateSecret[0]) )
		return Mismatch;

	Hash hash = Hash::Parse(tx.payload);
	const Byte* payload = hash.ToByteArray();

	if( 0!=memcmp(ram_txSeed, payload, Hash::Length) )
		return Mismatch;

	uint64_t secretSeed = Fnv64a( ram_crateSeed, CrateSecret::SEED_LENGTH, ram_randomSeed );

	uint64_t finalSeed = Fnv64a( ram_txSeed, ram_txSeed+Hash::Length, secretSeed );
	if( finalSeed != ram_finalSeed )
		return Mismatch;

	CrateInfo info;
	{
		CurlClient curl("");
		Char url[512];
		sprintf_s(url, PHANTASMA_LITERAL("https://www.22series.com/cdn/part_info/%") PRId64, itemId);
		CURLcode e = curl.Get(url);
		if( e != CURLE_OK )
		{
			return Indeterminate;
		}
		else 
		{
			info.itemId = itemId;

			char nil[1] = {'\0'};
			curl.result.append(nil, 1);
			JSONValue doc = json::Parse((JSONDocument)(char*)curl.result.begin());
			
			bool jsonError = false;
			const auto& groups = json::LookupArray(doc, PHANTASMA_LITERAL("groups"), jsonError);
			const auto& rolls = json::LookupArray(doc, PHANTASMA_LITERAL("rolls"), jsonError);
			if( jsonError )
				return Error;

			for(int i=0, end=json::ArraySize( groups, jsonError ); i!=end; ++i)
			{
				CrateInfo::ItemGroup groupInfo;
				auto groupValue = json::IndexArray(groups, i, jsonError);
				if( !json::IsArray(groupValue, jsonError) || jsonError )
					return Error;
				auto group = json::AsArray(groupValue, jsonError);
				for(int j=0, jend=json::ArraySize( group, jsonError ); j!=jend; ++j)
				{
					auto item = json::IndexArray(group, j, jsonError);
					uint32_t id = json::AsUInt32(item, jsonError);
					if( !id || jsonError )
						return Error;
					groupInfo.items.push_back(id);
				}
				info.groups.push_back(groupInfo);
			}

			for(int i=0, end=json::ArraySize( rolls, jsonError ); i!=end; ++i)
			{
				CrateInfo::ItemRoll rollInfo;
				auto rollValue = json::IndexArray(rolls, i, jsonError);
				if( !json::IsArray(rollValue, jsonError) || jsonError )
					return Error;
				auto roll = json::AsArray(rollValue, jsonError);
				for(int j=0, jend=json::ArraySize( roll, jsonError ); j!=jend; ++j)
				{
					auto item = json::IndexArray(roll, j, jsonError);
					uint32_t weight = json::AsUInt32(item, jsonError);
					if( jsonError )
						return Error;
					rollInfo.groupPercentages.push_back(weight);
				}
				info.rolls.push_back(rollInfo);
			}
		}
	}

	PHANTASMA_VECTOR<int64_t> itemsProduced;

	XorShift64M random(finalSeed);
	for(int i=0, end=(int)info.rolls.size(); i!=end; ++i)
	{
		const auto& weights = info.rolls[i].groupPercentages;
		uint64_t sum = 0;
		for(int j=0, jend=(int)weights.size(); j!=jend; ++j)
		{
			sum += weights[j];
		}
		eiASSERT( sum );
		if( !sum )
		{
			return Error;
		}
		uint64_t r = random.RandomIndex(sum);

		uint64_t cumulative = 0;
		int groupIndex = -1;
		for(int j=0, jend=(int)weights.size(); j!=jend; ++j)
		{
			cumulative += weights[j];
			if(r < cumulative)
			{
				groupIndex = j;
				break;
			}
		}
		eiASSERT( groupIndex >= 0 );
		if( groupIndex < 0 || groupIndex > (int)info.groups.size() )
		{
			return Error;
		}

		const auto& group = info.groups[groupIndex];
		if( group.items.empty() )
		{
			continue;
		}

		uint64_t itemIndex = random.RandomIndex(group.items.size());
		int64_t itemId = group.items[(size_t)itemIndex];
		itemsProduced.push_back(itemId);
	}


	PHANTASMA_VECTOR<int64_t> itemsMinted;
	for( const BigInteger& mintedID : nftIdsMinted )
	{
		auto data = api.GetTokenData("TTRS", mintedID.ToString().c_str(), &err);
		if( !GoatiNftRom_DecodeItem(Base16::Decode(data.rom), itemId, mintingSource, sourceData, 
			season, timestamp, randomSeed, counter, originalOwner,
			crateSecret, type) )
			return Error;
		itemsMinted.push_back(itemId);
	}

	if( itemsMinted.size() != itemsProduced.size() )
		return Mismatch;

	if( !PHANTASMA_EQUAL(itemsMinted.begin(), itemsMinted.end(), itemsProduced.begin()) )
		return Mismatch;

	return Match;
}
