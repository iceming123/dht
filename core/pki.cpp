#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#include "pki.h"
#include "tomcrypt.h"

static prng_state yarrow_prng;

bool make_key(const char* fpubkey, const char* fprvkey)
{
	unsigned char pubbuf[1024], prvbuf[1024];
	unsigned long pub, prv;
	ecc_key usera;
	ecc_make_key(&yarrow_prng, find_prng("yarrow"), 20, &usera);

	pub = sizeof(pubbuf);
	ecc_export(pubbuf, &pub, PK_PUBLIC, &usera);
	prv = sizeof(prvbuf);
	ecc_export(prvbuf, &prv, PK_PRIVATE, &usera);

	FILE* f = fopen(fpubkey, "w+");
	if (NULL == f)
		return 0;

	fwrite(pubbuf, pub, 1, f);
	fclose(f);

	f = fopen(fprvkey, "w+");
	if (NULL == f)
		return 0;

	fwrite(prvbuf, prv, 1, f);
	fclose(f);

	return 1;
}

bool verify(const unsigned char* cip, int clen, const unsigned char* ip, int ilen, const char* fpubkey)
{
	unsigned char pubbuf[1024] = {0};
	ecc_key pubKey;
	int stat, x = sizeof(pubbuf);

	FILE* f = fopen(fpubkey, "r+");
	if (NULL == f)
		return 0;

	fseek(f, 0L, SEEK_END);
	x = ftell(f);
	fseek(f, 0L, SEEK_SET);

	fread(pubbuf, x, 1, f);
	fclose(f);

	ecc_import(pubbuf, x, &pubKey);

	ecc_verify_hash(cip, clen, ip, ilen, &stat, &pubKey);
	return (bool)stat;
}

bool sign(const unsigned char* ip, int ilen, unsigned char* out, unsigned long clen, const char* fprvkey)
{
	unsigned char prvbuf[1024] = {0};
	ecc_key prvKey;
	int x = sizeof(prvbuf);

	FILE* f = fopen(fprvkey, "r+");
	if (NULL == f)
		return 0;

	fseek(f, 0L, SEEK_END);
	x = ftell(f);
	fseek(f, 0L, SEEK_SET);

	fread(prvbuf, x, 1, f);
	fclose(f);

	ecc_import(prvbuf, x, &prvKey);

	ecc_sign_hash(ip, ilen, out, &clen, &yarrow_prng, find_prng("yarrow"), &prvKey);

	return 1;
}

void reg_algs(void)
{
#ifdef LTC_RIJNDAEL
	register_cipher(&aes_desc);
#endif
#ifdef LTC_BLOWFISH
	register_cipher (&blowfish_desc);
#endif
#ifdef LTC_XTEA
	register_cipher(&xtea_desc);
#endif
#ifdef LTC_RC5
	register_cipher(&rc5_desc);
#endif
#ifdef LTC_RC6
	register_cipher(&rc6_desc);
#endif
#ifdef LTC_TWOFISH
	register_cipher(&twofish_desc);
#endif
#ifdef LTC_RC2
	register_cipher(&rc2_desc);
#endif
#ifdef LTC_DES
	register_cipher(&des_desc);
	register_cipher(&des3_desc);
#endif
#ifdef LTC_CAST5
	register_cipher(&cast5_desc);
#endif
#ifdef LTC_NOEKEON
	register_cipher(&noekeon_desc);
#endif
#ifdef LTC_SKIPJACK
	register_cipher(&skipjack_desc);
#endif
#ifdef LTC_KHAZAD
	register_cipher(&khazad_desc);
#endif
#ifdef LTC_ANUBIS
	register_cipher(&anubis_desc);
#endif

	if (register_hash(&sha256_desc) == -1) {
		printf("Error registering LTC_SHA256\n");
		exit(-1);
	}

	if (register_prng(&yarrow_desc) == -1) {
		printf("Error registering yarrow PRNG\n");
		exit(-1);
	}

	if (register_prng(&sprng_desc) == -1) {
		printf("Error registering sprng PRNG\n");
		exit(-1);
	}

#ifdef USE_LTM
	ltc_mp = ltm_desc;
#endif

	if ((errno = rng_make_prng(128, find_prng("yarrow"), &yarrow_prng, NULL)) != CRYPT_OK) {
		printf("Error setting up PRNG, %s\n", error_to_string(errno));
	}
}

bool selftest()
{
	reg_algs();

	make_key("pubkey", "prvkey");

	char* ip = "192.168.1.1";
	unsigned char out[1024];
	int clen = 1024, iplen = sizeof(ip);

	sign((unsigned char*)ip, sizeof(ip), out, clen, "prvkey");

	return verify(out, clen, (unsigned char*)ip, iplen, "pubkey");
}

/*
int main(int argc, char* argv[])
{
	selftest();
	return 1;
}
*/