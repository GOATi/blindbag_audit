
constexpr static int GOATi_Nft_Version = 0;
enum class GOATiNftType
{
	System = 0,
	Message = 1,
	Item = 2,
	Vehicle = 3,
	Crate = 4,
	License = 5,
	Bundle = 6,
	Badge = 7,
};

enum class GOATiNftSystemSubType
{
	NftMintingState = 0,
	AuctionState = 1,
};

enum class GOATiNftMintingSource
{
	Store = 0,
};

enum class GOATiNftSeason
{
	PreSeason = 0,
};

namespace GOATiNftMintingSourceStore { enum Type {
	PurchaseGOATI = 0,
	PurchaseSOUL = 1,
	Crate = 2,
	PurchaseCoin = 3,
};}

inline GOATiNftType TypeFromItemId(int64_t itemId)
{
	int64_t high_bytes = itemId >> 24;
	if( high_bytes == 0 )
		return GOATiNftType::Item;
	if( high_bytes == 1 )
		return GOATiNftType::Vehicle;
	if( high_bytes == 2 )
		return GOATiNftType::Crate;
	if( high_bytes == 3 )
		return GOATiNftType::License;
	if( high_bytes == 4 )
		return GOATiNftType::Bundle;
	if( high_bytes == 5 )
		return GOATiNftType::Badge;
	eiASSERT( false );
	return GOATiNftType::System;
}

inline int64_t RandomSeed64()
{
	Byte randomSeed_buf[8];
	PHANTASMA_RANDOMBYTES(randomSeed_buf, 8);
	int64_t randomSeed;
	memcpy(&randomSeed, randomSeed_buf, 8);
	return randomSeed;
}

struct CrateSecret
{
	static constexpr int SEED_LENGTH = sizeof(int64_t)*3;
	static constexpr int SALT_LENGTH = sizeof(int64_t);
	static constexpr int MESSAGE_LENGTH = SEED_LENGTH + SALT_LENGTH;
	static constexpr int HASH_LENGTH = PHANTASMA_SHA256_LENGTH;
	static constexpr int ENCRYPTION_OVERHEAD = crypto_secretbox_MACBYTES;
	static constexpr int ENCRYPTED_LENGTH = MESSAGE_LENGTH + ENCRYPTION_OVERHEAD;
	static constexpr int NONCE_LENGTH = PHANTASMA_AuthenticatedNonceLength;
	static constexpr int LENGTH = HASH_LENGTH + ENCRYPTED_LENGTH + NONCE_LENGTH;
	int64_t randomSeed[3];
	int64_t salt;
	Byte data[LENGTH];
};

inline CrateSecret MakeCrateSecret(const Byte* encryptionKey)
{
	CrateSecret result;
	for( int i=0; i!=CrateSecret::SEED_LENGTH/sizeof(int64_t); ++i )
		result.randomSeed[i] = RandomSeed64();
	result.salt = RandomSeed64();
	Byte bytesToHash[CrateSecret::MESSAGE_LENGTH];
	memcpy(bytesToHash, &result.randomSeed[0], CrateSecret::SEED_LENGTH);
	memcpy(bytesToHash+CrateSecret::SEED_LENGTH, &result.salt, CrateSecret::SALT_LENGTH);
	PHANTASMA_SHA256(result.data, CrateSecret::HASH_LENGTH, bytesToHash, CrateSecret::MESSAGE_LENGTH);

	Byte nonce[CrateSecret::NONCE_LENGTH];
	PHANTASMA_RANDOMBYTES(nonce, CrateSecret::NONCE_LENGTH);
	int code = Phantasma_Encrypt(result.data + CrateSecret::HASH_LENGTH, CrateSecret::ENCRYPTED_LENGTH, (Byte*)bytesToHash, CrateSecret::MESSAGE_LENGTH, nonce, encryptionKey);
	eiASSERT( code == 0 );
	memcpy(result.data + CrateSecret::HASH_LENGTH + CrateSecret::ENCRYPTED_LENGTH, nonce, CrateSecret::NONCE_LENGTH);

	return result;
}
inline bool ValidateCrateSecret(int64_t salt, uint64_t* secrets, const Byte* sha256)
{
	Byte bytesToHash[CrateSecret::MESSAGE_LENGTH];
	memcpy(bytesToHash, secrets, CrateSecret::SEED_LENGTH);
	memcpy(bytesToHash+CrateSecret::SEED_LENGTH, &salt, CrateSecret::SALT_LENGTH);
	Byte computed_sha256[CrateSecret::HASH_LENGTH];
	PHANTASMA_SHA256(computed_sha256, CrateSecret::HASH_LENGTH, bytesToHash, CrateSecret::MESSAGE_LENGTH);
	if( !PHANTASMA_EQUAL(sha256, sha256+CrateSecret::HASH_LENGTH, computed_sha256) )
		return false;
	return true;
}
inline bool DecodeCrateSecret(CrateSecret& out_result, const Byte* sha256, const Byte* encrypted, const Byte* encryptionKey)
{
	Byte resultBytes[CrateSecret::MESSAGE_LENGTH];
	constexpr int len = CrateSecret::ENCRYPTED_LENGTH;
	int result = Phantasma_Decrypt(resultBytes, CrateSecret::MESSAGE_LENGTH, encrypted, len, &encrypted[len], encryptionKey);
	if( result != 0 )
		return false;
	Byte decrypted_sha256[CrateSecret::HASH_LENGTH];
	PHANTASMA_SHA256(decrypted_sha256, CrateSecret::HASH_LENGTH, resultBytes, CrateSecret::MESSAGE_LENGTH);
	if( !PHANTASMA_EQUAL(sha256, sha256+CrateSecret::HASH_LENGTH, decrypted_sha256) )
		return false;
	memcpy(&out_result.randomSeed[0], resultBytes, CrateSecret::SEED_LENGTH);
	memcpy(&out_result.salt, resultBytes+CrateSecret::SEED_LENGTH, CrateSecret::SALT_LENGTH);
	return true;
}
inline bool DecodeCrateSecret(CrateSecret& out_result, const Byte* data, const Byte* encryptionKey)
{
	return DecodeCrateSecret(out_result, data, data+CrateSecret::HASH_LENGTH, encryptionKey);
}
inline bool DecodeCrateSecret(CrateSecret& inout_result, const Byte* encryptionKey)
{
	return DecodeCrateSecret(inout_result, inout_result.data, inout_result.data+CrateSecret::HASH_LENGTH, encryptionKey);
}


inline bool GenerateSerialKey(char* key, int length)
{
	if( length != 20 )
		return false;
	constexpr const char alphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ234567890";
	PHANTASMA_RANDOMBYTES(key, 4*5);
	for(int i = 0; i != 4; ++i)
	{
		key[i*5+0] = alphabet[((Byte)key[i*5+0])%32];
		key[i*5+1] = alphabet[((Byte)key[i*5+1])%32];
		key[i*5+2] = alphabet[((Byte)key[i*5+2])%32];
		key[i*5+3] = alphabet[((Byte)key[i*5+3])%32];
		key[i*5+4] = '-';
	}
	key[4*5-1] = '\0';
	return true;
}

inline bool GenerateEncryptedSerialKey(ByteArray& output, const Byte* encryptionKey)
{
	char serialKey[20];
	bool ok = GenerateSerialKey(serialKey, 20);
	if( !ok )
		return false;
	Byte nonce[PHANTASMA_AuthenticatedNonceLength];
	PHANTASMA_RANDOMBYTES(nonce, PHANTASMA_AuthenticatedNonceLength);
	int len = Phantasma_Encrypt(0, 0, (Byte*)serialKey, 20, nonce, encryptionKey);
	if( len < 0 )
		return false;
	output.resize(len + PHANTASMA_AuthenticatedNonceLength);
	len = Phantasma_Encrypt(&output.front(), len, (Byte*)serialKey, 20, nonce, encryptionKey);
	if( len == 0 )
		memcpy(&output[len], nonce, PHANTASMA_AuthenticatedNonceLength);
	return len == 0;
}

inline ByteArray GoatiNftRom_Auction(const Address& auctionWallet)
{
	BinaryWriter w(256);
	w.Write((Byte)GOATi_Nft_Version);
	w.Write((Byte)GOATiNftType::System);
	w.Write((Byte)GOATiNftSystemSubType::AuctionState);
	w.WriteByteArray(auctionWallet.ToByteArray(), Address::LengthInBytes);
	return w.ToArray();
}

inline ByteArray GoatiNftRom_Item(const Address& originalOwner, GOATiNftType type, int64_t itemId, GOATiNftMintingSource mintingSource, int32_t sourceData,
	GOATiNftSeason season, int32_t timestamp, int64_t randomSeed, const Byte* extraData, int extraSize)
{
	eiASSERT( type == TypeFromItemId(itemId) );
	eiASSERT( type != GOATiNftType::Bundle );
	BinaryWriter w(256);
	w.Write((Byte)GOATi_Nft_Version);
	w.Write((Byte)type);
	w.Write(itemId);
	w.Write((Byte)mintingSource);
	w.Write(sourceData);
	w.Write((uint32_t)season);
	w.Write(timestamp);
	w.Write(randomSeed);
	w.WriteByteArray(originalOwner.ToByteArray(), Address::LengthInBytes);
	if(type == GOATiNftType::Crate)
	{
		eiASSERT( extraData && extraSize == CrateSecret::LENGTH );
		w.WriteByteArray(extraData, extraSize);
	}
	else if(type == GOATiNftType::License)
	{
		eiASSERT( extraData && extraSize == 20 + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES );
		w.WriteByteArray(extraData, extraSize);
	}
	return w.ToArray();
}
inline bool GoatiNftRom_DecodeItem(const ByteArray& rom, int64_t& out_itemId, GOATiNftMintingSource& out_mintingSource, int32_t& out_sourceData, 
	GOATiNftSeason& out_season, int32_t& out_timestamp, int64_t& out_randomSeed, BigInteger& out_counter, Address& out_originalOwner,
	ByteArray& out_crateSecretHash, GOATiNftType& out_type)
{
	BinaryReader r(rom);
	Byte version = r.ReadByte();
	if(version == GOATi_Nft_Version)
	{
		Byte type = r.ReadByte();
		if(type == (Byte)GOATiNftType::Item || type == (Byte)GOATiNftType::Vehicle || type == (Byte)GOATiNftType::Crate || type == (Byte)GOATiNftType::License)
		{
			uint32_t season;
			Byte mintingSource;
			Byte originalOwner[Address::LengthInBytes];
			r.Read(out_itemId);//8
			r.Read(mintingSource);//1
			r.Read(out_sourceData);//4
			r.Read(season);//4
			r.Read(out_timestamp);//4
			r.Read(out_randomSeed);//8
			r.ReadByteArray(originalOwner);//34
			if(type == (Byte)GOATiNftType::Crate)
			{
				out_crateSecretHash.resize(CrateSecret::LENGTH);
				r.ReadByteArray(&out_crateSecretHash.front(), CrateSecret::LENGTH);
			}
			else if(type == (Byte)GOATiNftType::License)
			{
				out_crateSecretHash.resize(20 + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES);
				r.ReadByteArray(&out_crateSecretHash.front(), 20 + crypto_secretbox_MACBYTES + crypto_secretbox_NONCEBYTES);
			}
			else
				out_crateSecretHash.clear();

			if( !r.Error() )
			{
				uint32_t cursor = r.Position();
				const Byte* data = &rom[cursor];
				int dataLength = (int)rom.size() - (int)cursor;
				out_counter = BigInteger::FromSignedArray(data, dataLength);
				out_mintingSource = (GOATiNftMintingSource)mintingSource;
				out_season = (GOATiNftSeason)season;
				out_originalOwner = Address(originalOwner, Address::LengthInBytes);
				out_type = (GOATiNftType)type;
				return true;
			}
		}
	}
	return false;
}

inline ByteArray GoatiNftRom_NftMintingState(int64_t itemId, const Address& tokenOwner, int64_t mintLimit, int64_t randomSeed)
{
	eiASSERT( mintLimit >= 0 && mintLimit <= 0xFFFFFFFFULL );
	BinaryWriter w(256);
	w.Write((Byte)GOATi_Nft_Version);
	w.Write((Byte)GOATiNftType::System);
	w.Write((Byte)GOATiNftSystemSubType::NftMintingState);
	w.Write(itemId);
	w.Write(tokenOwner.ToByteArray(), Address::LengthInBytes);
	w.Write(mintLimit);
	w.Write(randomSeed);
	return w.ToArray();
}
inline bool GoatiNftRom_DecodeNftMintingState(const ByteArray& rom, int64_t& out_itemId, int64_t& out_mintLimit)
{
	BinaryReader r(rom);
	Byte version = r.ReadByte();
	if(version == GOATi_Nft_Version)
	{
		Byte type = r.ReadByte();
		Byte system = r.ReadByte();
		if(type == (Byte)GOATiNftType::System && system == (Byte)GOATiNftSystemSubType::NftMintingState)
		{
			r.Read(out_itemId);
			Byte owner[Address::LengthInBytes];
			r.Read(owner, Address::LengthInBytes);
			r.Read(out_mintLimit);
			return !r.Error();
		}
	}
	return false;
}





inline ScriptBuilder& IncrementRAM(ScriptBuilder& sb, const String& tokenSymbol, const BigInteger& nftId, Byte temp_reg = 1)
{
	return sb
		// temp_reg = Runtime.ReadToken(tokenSymbol, nftId) -> byte[]
		.ReadTokenToRegister(tokenSymbol, nftId, temp_reg)
		// temp_reg = (Number)temp_reg
		.EmitCast(temp_reg, temp_reg, VMType::Number)
		// temp_reg++
		.EmitInc(temp_reg)
		// temp_reg = (byte[])temp_reg
		.EmitCast(temp_reg, temp_reg, VMType::Bytes)
		// Runtime.WriteToken(tokenSymbol, nftId, temp_reg) -> void
		.WriteTokenFromRegister(tokenSymbol, nftId, temp_reg);
}

inline ScriptBuilder& MintTokenIncrementingId( ScriptBuilder& sb, const BigInteger& stateNftId, const String& tokenSymbol, const Address& from, const Address& to, const ByteArray& rom, const ByteArray& ram )
{
	Byte counter_reg = 1;
	Byte rom_reg = 2;
	Byte ram_reg = 0;
	// counter_reg = ++ nftId's RAM as byte[]
	IncrementRAM(sb, tokenSymbol, stateNftId, counter_reg);

	//EmitGreaterThan(src_a, src_b, dst_reg)
	//ScriptBuilder& EmitThrow( const ByteArray& data )
	//ScriptBuilder& EmitLabel( const String& label )
	//ScriptBuilder& EmitJump( Opcode opcode, const String& label, Byte reg = 0 )

	return sb
		// rom_reg = rom
		.EmitLoad(rom_reg, rom)
		//rom_reg = rom_reg.Concatenate(counter_reg)
		.EmitCat(rom_reg, counter_reg, rom_reg)
		// ram_reg = ram
		.EmitLoad(ram_reg, ram)
		//Runtime.MintToken(tokenSymbol, from, to, rom_reg, ram_reg)
		.MintTokenContentsFromRegisters(tokenSymbol, from, to, rom_reg, ram_reg);
}

inline ScriptBuilder& MintItem(ScriptBuilder& sb, const Address& tokenAddr, const Address& to, int64_t itemId, BigInteger stateNftId,
	GOATiNftMintingSource source, int32_t sourceData, GOATiNftSeason season, Timestamp time, int64_t salt, const Byte* extraData, int extraSize)
{
	GOATiNftType type = TypeFromItemId(itemId);
	ByteArray zero = BigInteger::Zero().ToSignedByteArray();
	ByteArray nftRom = GoatiNftRom_Item(to, type, itemId, source, sourceData, season, time.Value, salt, extraData, extraSize);
	return MintTokenIncrementingId(sb, stateNftId, String("TTRS"), tokenAddr, to, nftRom, zero);
}
