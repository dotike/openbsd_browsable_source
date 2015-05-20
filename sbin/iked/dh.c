/*	$OpenBSD: dh.c,v 1.16 2015/01/16 06:39:58 deraadt Exp $	*/

/*
 * Copyright (c) 2010-2014 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>	/* roundup */
#include <string.h>

#include <openssl/obj_mac.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/bn.h>

#include "dh.h"

int	dh_init(struct group *);

/* MODP */
int	modp_init(struct group *);
int	modp_getlen(struct group *);
int	modp_create_exchange(struct group *, u_int8_t *);
int	modp_create_shared(struct group *, u_int8_t *, u_int8_t *);

/* EC2N/ECP */
int	ec_init(struct group *);
int	ec_getlen(struct group *);
int	ec_create_exchange(struct group *, u_int8_t *);
int	ec_create_shared(struct group *, u_int8_t *, u_int8_t *);

int	ec_point2raw(struct group *, const EC_POINT *, u_int8_t *, size_t);
EC_POINT *
	ec_raw2point(struct group *, u_int8_t *, size_t);

/* curve25519 */
int	ec25519_init(struct group *);
int	ec25519_getlen(struct group *);
int	ec25519_create_exchange(struct group *, u_int8_t *);
int	ec25519_create_shared(struct group *, u_int8_t *, u_int8_t *);

#define CURVE25519_SIZE 32	/* 256 bits */
struct curve25519_key {
	u_int8_t	 secret[CURVE25519_SIZE];
	u_int8_t	 public[CURVE25519_SIZE];
};
extern int crypto_scalarmult_curve25519(u_char a[CURVE25519_SIZE],
    const u_char b[CURVE25519_SIZE], const u_char c[CURVE25519_SIZE])
	__attribute__((__bounded__(__minbytes__, 1, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 2, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 3, CURVE25519_SIZE)));

struct group_id ike_groups[] = {
	{ GROUP_MODP, 1, 768,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A63A3620FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 2, 1024,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381"
	    "FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_EC2N, 3, 155, NULL, NULL, NID_ipsec3 },
	{ GROUP_EC2N, 4, 185, NULL, NULL, NID_ipsec4 },
	{ GROUP_MODP, 5, 1536,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 14, 2048,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
	    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
	    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
	    "15728E5A8AACAA68FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 15, 3072,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
	    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
	    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
	    "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
	    "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
	    "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
	    "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
	    "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
	    "43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 16, 4096,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
	    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
	    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
	    "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
	    "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
	    "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
	    "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
	    "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
	    "43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
	    "88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
	    "2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
	    "287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
	    "1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
	    "93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934063199"
	    "FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 17, 6144,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
	    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
	    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
	    "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
	    "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
	    "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
	    "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
	    "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
	    "43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
	    "88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
	    "2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
	    "287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
	    "1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
	    "93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492"
	    "36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD"
	    "F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831"
	    "179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
	    "DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF"
	    "5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6"
	    "D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3"
	    "23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA"
	    "CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328"
	    "06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C"
	    "DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE"
	    "12BF2D5B0B7474D6E694F91E6DCC4024FFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_MODP, 18, 8192,
	    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
	    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
	    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
	    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
	    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
	    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
	    "83655D23DCA3AD961C62F356208552BB9ED529077096966D"
	    "670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
	    "E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
	    "DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
	    "15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
	    "ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
	    "ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
	    "F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
	    "BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
	    "43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
	    "88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
	    "2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
	    "287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
	    "1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
	    "93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492"
	    "36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD"
	    "F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831"
	    "179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
	    "DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF"
	    "5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6"
	    "D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3"
	    "23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA"
	    "CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328"
	    "06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C"
	    "DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE"
	    "12BF2D5B0B7474D6E694F91E6DBE115974A3926F12FEE5E4"
	    "38777CB6A932DF8CD8BEC4D073B931BA3BC832B68D9DD300"
	    "741FA7BF8AFC47ED2576F6936BA424663AAB639C5AE4F568"
	    "3423B4742BF1C978238F16CBE39D652DE3FDB8BEFC848AD9"
	    "22222E04A4037C0713EB57A81A23F0C73473FC646CEA306B"
	    "4BCBC8862F8385DDFA9D4B7FA2C087E879683303ED5BDD3A"
	    "062B3CF5B3A278A66D2A13F83F44F82DDF310EE074AB6A36"
	    "4597E899A0255DC164F31CC50846851DF9AB48195DED7EA1"
	    "B1D510BD7EE74D73FAF36BC31ECFA268359046F4EB879F92"
	    "4009438B481C6CD7889A002ED5EE382BC9190DA6FC026E47"
	    "9558E4475677E9AA9E3050E2765694DFC81F56E880B96E71"
	    "60C980DD98EDD3DFFFFFFFFFFFFFFFFF",
	    "02"
	},
	{ GROUP_ECP, 19, 256, NULL, NULL, NID_X9_62_prime256v1 },
	{ GROUP_ECP, 20, 384, NULL, NULL, NID_secp384r1 },
	{ GROUP_ECP, 21, 521, NULL, NULL, NID_secp521r1 },
	{ GROUP_MODP, 22, 1024,
	    "B10B8F96A080E01DDE92DE5EAE5D54EC52C99FBCFB06A3C6"
	    "9A6A9DCA52D23B616073E28675A23D189838EF1E2EE652C0"
	    "13ECB4AEA906112324975C3CD49B83BFACCBDD7D90C4BD70"
	    "98488E9C219A73724EFFD6FAE5644738FAA31A4FF55BCCC0"
	    "A151AF5F0DC8B4BD45BF37DF365C1A65E68CFDA76D4DA708"
	    "DF1FB2BC2E4A4371",
	    "A4D1CBD5C3FD34126765A442EFB99905F8104DD258AC507F"
	    "D6406CFF14266D31266FEA1E5C41564B777E690F5504F213"
	    "160217B4B01B886A5E91547F9E2749F4D7FBD7D3B9A92EE1"
	    "909D0D2263F80A76A6A24C087A091F531DBF0A0169B6A28A"
	    "D662A4D18E73AFA32D779D5918D08BC8858F4DCEF97C2A24"
	    "855E6EEB22B3B2E5"
	},
	{ GROUP_MODP, 23, 2048,
	    "AD107E1E9123A9D0D660FAA79559C51FA20D64E5683B9FD1"
	    "B54B1597B61D0A75E6FA141DF95A56DBAF9A3C407BA1DF15"
	    "EB3D688A309C180E1DE6B85A1274A0A66D3F8152AD6AC212"
	    "9037C9EDEFDA4DF8D91E8FEF55B7394B7AD5B7D0B6C12207"
	    "C9F98D11ED34DBF6C6BA0B2C8BBC27BE6A00E0A0B9C49708"
	    "B3BF8A317091883681286130BC8985DB1602E714415D9330"
	    "278273C7DE31EFDC7310F7121FD5A07415987D9ADC0A486D"
	    "CDF93ACC44328387315D75E198C641A480CD86A1B9E587E8"
	    "BE60E69CC928B2B9C52172E413042E9B23F10B0E16E79763"
	    "C9B53DCF4BA80A29E3FB73C16B8E75B97EF363E2FFA31F71"
	    "CF9DE5384E71B81C0AC4DFFE0C10E64F",
	    "AC4032EF4F2D9AE39DF30B5C8FFDAC506CDEBE7B89998CAF"
	    "74866A08CFE4FFE3A6824A4E10B9A6F0DD921F01A70C4AFA"
	    "AB739D7700C29F52C57DB17C620A8652BE5E9001A8D66AD7"
	    "C17669101999024AF4D027275AC1348BB8A762D0521BC98A"
	    "E247150422EA1ED409939D54DA7460CDB5F6C6B250717CBE"
	    "F180EB34118E98D119529A45D6F834566E3025E316A330EF"
	    "BB77A86F0C1AB15B051AE3D428C8F8ACB70A8137150B8EEB"
	    "10E183EDD19963DDD9E263E4770589EF6AA21E7F5F2FF381"
	    "B539CCE3409D13CD566AFBB48D6C019181E1BCFE94B30269"
	    "EDFE72FE9B6AA4BD7B5A0F1C71CFFF4C19C418E1F6EC0179"
	    "81BC087F2A7065B384B890D3191F2BFA"
	},
	{ GROUP_MODP, 24, 2048,
	    "87A8E61DB4B6663CFFBBD19C651959998CEEF608660DD0F2"
	    "5D2CEED4435E3B00E00DF8F1D61957D4FAF7DF4561B2AA30"
	    "16C3D91134096FAA3BF4296D830E9A7C209E0C6497517ABD"
	    "5A8A9D306BCF67ED91F9E6725B4758C022E0B1EF4275BF7B"
	    "6C5BFC11D45F9088B941F54EB1E59BB8BC39A0BF12307F5C"
	    "4FDB70C581B23F76B63ACAE1CAA6B7902D52526735488A0E"
	    "F13C6D9A51BFA4AB3AD8347796524D8EF6A167B5A41825D9"
	    "67E144E5140564251CCACB83E6B486F6B3CA3F7971506026"
	    "C0B857F689962856DED4010ABD0BE621C3A3960A54E710C3"
	    "75F26375D7014103A4B54330C198AF126116D2276E11715F"
	    "693877FAD7EF09CADB094AE91E1A1597",
	    "3FB32C9B73134D0B2E77506660EDBD484CA7B18F21EF2054"
	    "07F4793A1A0BA12510DBC15077BE463FFF4FED4AAC0BB555"
	    "BE3A6C1B0C6B47B1BC3773BF7E8C6F62901228F8C28CBB18"
	    "A55AE31341000A650196F931C77A57F2DDF463E5E9EC144B"
	    "777DE62AAAB8A8628AC376D282D6ED3864E67982428EBC83"
	    "1D14348F6F2F9193B5045AF2767164E1DFC967C1FB3F2E55"
	    "A4BD1BFFE83B9C80D052B985D182EA0ADB2A3B7313D3FE14"
	    "C8484B1E052588B9B7D2BBD2DF016199ECD06E1557CD0915"
	    "B3353BBB64E0EC377FD028370DF92B52C7891428CDC67EB6"
	    "184B523D1DB246C32F63078490F00EF8D647D148D4795451"
	    "5E2327CFEF98C582664B4C0F6CC41659"
	},
	{ GROUP_ECP, 25, 192, NULL, NULL, NID_X9_62_prime192v1 },
	{ GROUP_ECP, 26, 224, NULL, NULL, NID_secp224r1 },
	{ GROUP_ECP, 27, 224, NULL, NULL, NID_brainpoolP224r1 },
	{ GROUP_ECP, 28, 256, NULL, NULL, NID_brainpoolP256r1 },
	{ GROUP_ECP, 29, 384, NULL, NULL, NID_brainpoolP384r1 },
	{ GROUP_ECP, 30, 512, NULL, NULL, NID_brainpoolP512r1 },

	/* "Private use" extensions */
	{ GROUP_CURVE25519, 1034, CURVE25519_SIZE * 8 }
};

void
group_init(void)
{
	/* currently not used */
	return;
}

void
group_free(struct group *group)
{
	if (group == NULL)
		return;
	if (group->dh != NULL)
		DH_free(group->dh);
	if (group->ec != NULL)
		EC_KEY_free(group->ec);
	if (group->curve25519 != NULL) {
		explicit_bzero(group->curve25519,
		    sizeof(struct curve25519_key));
		free(group->curve25519);
	}
	group->spec = NULL;
	free(group);
}

struct group *
group_get(u_int32_t id)
{
	struct group_id	*p = NULL;
	struct group	*group;
	u_int		 i, items;

	items = sizeof(ike_groups) / sizeof(ike_groups[0]);
	for (i = 0; i < items; i++) {
		if (id == ike_groups[i].id) {
			p = &ike_groups[i];
			break;
		}
	}
	if (p == NULL)
		return (NULL);

	if ((group = calloc(1, sizeof(*group))) == NULL)
		return (NULL);

	group->id = id;
	group->spec = p;

	switch (p->type) {
	case GROUP_MODP:
		group->init = modp_init;
		group->getlen = modp_getlen;
		group->exchange = modp_create_exchange;
		group->shared = modp_create_shared;
		break;
	case GROUP_EC2N:
	case GROUP_ECP:
		group->init = ec_init;
		group->getlen = ec_getlen;
		group->exchange = ec_create_exchange;
		group->shared = ec_create_shared;
		break;
	case GROUP_CURVE25519:
		group->init = ec25519_init;
		group->getlen = ec25519_getlen;
		group->exchange = ec25519_create_exchange;
		group->shared = ec25519_create_shared;
		break;
	default:
		group_free(group);
		return (NULL);
	}

	if (dh_init(group) != 0) {
		group_free(group);
		return (NULL);
	}

	return (group);
}

int
dh_init(struct group *group)
{
	return (group->init(group));
}

int
dh_getlen(struct group *group)
{
	return (group->getlen(group));
}

int
dh_create_exchange(struct group *group, u_int8_t *buf)
{
	return (group->exchange(group, buf));
}

int
dh_create_shared(struct group *group, u_int8_t *secret, u_int8_t *exchange)
{
	return (group->shared(group, secret, exchange));
}

int
modp_init(struct group *group)
{
	DH	*dh;

	if ((dh = DH_new()) == NULL)
		return (-1);
	group->dh = dh;

	if (!BN_hex2bn(&dh->p, group->spec->prime) ||
	    !BN_hex2bn(&dh->g, group->spec->generator))
		return (-1);

	return (0);
}

int
modp_getlen(struct group *group)
{
	if (group->spec == NULL)
		return (0);
	return (roundup(group->spec->bits, 8) / 8);
}

int
modp_create_exchange(struct group *group, u_int8_t *buf)
{
	DH	*dh = group->dh;
	int	 len, ret;

	if (!DH_generate_key(dh))
		return (-1);
	ret = BN_bn2bin(dh->pub_key, buf);
	if (!ret)
		return (-1);

	len = dh_getlen(group);

	/* add zero padding */
	if (ret < len) {
		bcopy(buf, buf + (len - ret), ret);
		bzero(buf, len - ret);
	}

	return (0);
}

int
modp_create_shared(struct group *group, u_int8_t *secret, u_int8_t *exchange)
{
	BIGNUM	*ex;
	int	 len, ret;

	len = dh_getlen(group);

	if ((ex = BN_bin2bn(exchange, len, NULL)) == NULL)
		return (-1);

	ret = DH_compute_key(secret, ex, group->dh);
	BN_clear_free(ex);
	if (ret <= 0)
		return (-1);

	/* add zero padding */
	if (ret < len) {
		bcopy(secret, secret + (len - ret), ret);
		bzero(secret, len - ret);
	}

	return (0);
}

int
ec_init(struct group *group)
{
	if ((group->ec = EC_KEY_new_by_curve_name(group->spec->nid)) == NULL)
		return (-1);
	if (!EC_KEY_generate_key(group->ec))
		return (-1);
	if (!EC_KEY_check_key(group->ec)) {
		EC_KEY_free(group->ec);
		return (-1);
	}
	return (0);
}

int
ec_getlen(struct group *group)
{
	if (group->spec == NULL)
		return (0);
	/* NB:  Return value will always be even */
	return ((roundup(group->spec->bits, 8) * 2) / 8);
}

int
ec_create_exchange(struct group *group, u_int8_t *buf)
{
	size_t	 len;

	len = ec_getlen(group);
	bzero(buf, len);

	return (ec_point2raw(group, EC_KEY_get0_public_key(group->ec),
	    buf, len));
}

int
ec_create_shared(struct group *group, u_int8_t *secret, u_int8_t *exchange)
{
	const EC_GROUP	*ecgroup = NULL;
	const BIGNUM	*privkey;
	EC_KEY		*exkey = NULL;
	EC_POINT	*exchangep = NULL, *secretp = NULL;
	int		 ret = -1;

	if ((ecgroup = EC_KEY_get0_group(group->ec)) == NULL ||
	    (privkey = EC_KEY_get0_private_key(group->ec)) == NULL)
		goto done;

	if ((exchangep =
	    ec_raw2point(group, exchange, ec_getlen(group))) == NULL)
		goto done;

	if ((exkey = EC_KEY_new()) == NULL)
		goto done;
	if (!EC_KEY_set_group(exkey, ecgroup))
		goto done;
	if (!EC_KEY_set_public_key(exkey, exchangep))
		goto done;

	/* validate exchangep */
	if (!EC_KEY_check_key(exkey))
		goto done;

	if ((secretp = EC_POINT_new(ecgroup)) == NULL)
		goto done;

	if (!EC_POINT_mul(ecgroup, secretp, NULL, exchangep, privkey, NULL))
		goto done;

	ret = ec_point2raw(group, secretp, secret, ec_getlen(group));

 done:
	if (exkey != NULL)
		EC_KEY_free(exkey);
	if (exchangep != NULL)
		EC_POINT_clear_free(exchangep);
	if (secretp != NULL)
		EC_POINT_clear_free(secretp);

	return (ret);
}

int
ec_point2raw(struct group *group, const EC_POINT *point,
    u_int8_t *buf, size_t len)
{
	const EC_GROUP	*ecgroup = NULL;
	BN_CTX		*bnctx = NULL;
	BIGNUM		*x = NULL, *y = NULL;
	int		 ret = -1;
	size_t		 eclen, xlen, ylen;
	off_t		 xoff, yoff;

	if ((bnctx = BN_CTX_new()) == NULL)
		goto done;
	BN_CTX_start(bnctx);
	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL)
		goto done;

	eclen = ec_getlen(group);
	if (len < eclen)
		goto done;
	xlen = ylen = eclen / 2;

	if ((ecgroup = EC_KEY_get0_group(group->ec)) == NULL)
		goto done;

	if (EC_METHOD_get_field_type(EC_GROUP_method_of(ecgroup)) ==
	    NID_X9_62_prime_field) {
		if (!EC_POINT_get_affine_coordinates_GFp(ecgroup,
		    point, x, y, bnctx))
			goto done;
	} else {
		if (!EC_POINT_get_affine_coordinates_GF2m(ecgroup,
		    point, x, y, bnctx))
			goto done;
	}

	xoff = xlen - BN_num_bytes(x);
	bzero(buf, xoff);
	if (!BN_bn2bin(x, buf + xoff))
		goto done;

	yoff = (ylen - BN_num_bytes(y)) + xlen;
	bzero(buf + xlen, yoff - xlen);
	if (!BN_bn2bin(y, buf + yoff))
		goto done;

	ret = 0;
 done:
	/* Make sure to erase sensitive data */
	if (x != NULL)
		BN_clear(x);
	if (y != NULL)
		BN_clear(y);
	BN_CTX_end(bnctx);
	BN_CTX_free(bnctx);

	return (ret);
}

EC_POINT *
ec_raw2point(struct group *group, u_int8_t *buf, size_t len)
{
	const EC_GROUP	*ecgroup = NULL;
	EC_POINT	*point = NULL;
	BN_CTX		*bnctx = NULL;
	BIGNUM		*x = NULL, *y = NULL;
	int		 ret = -1;
	size_t		 eclen;
	size_t		 xlen, ylen;

	if ((bnctx = BN_CTX_new()) == NULL)
		goto done;
	BN_CTX_start(bnctx);
	if ((x = BN_CTX_get(bnctx)) == NULL ||
	    (y = BN_CTX_get(bnctx)) == NULL)
		goto done;

	eclen = ec_getlen(group);
	if (len < eclen)
		goto done;
	xlen = ylen = eclen / 2;
	if ((x = BN_bin2bn(buf, xlen, x)) == NULL ||
	    (y = BN_bin2bn(buf + xlen, ylen, y)) == NULL)
		goto done;

	if ((ecgroup = EC_KEY_get0_group(group->ec)) == NULL)
		goto done;

	if ((point = EC_POINT_new(ecgroup)) == NULL)
		goto done;

	if (EC_METHOD_get_field_type(EC_GROUP_method_of(ecgroup)) ==
	    NID_X9_62_prime_field) {
		if (!EC_POINT_set_affine_coordinates_GFp(ecgroup,
		    point, x, y, bnctx))
			goto done;
	} else {
		if (!EC_POINT_set_affine_coordinates_GF2m(ecgroup,
		    point, x, y, bnctx))
			goto done;
	}

	ret = 0;
 done:
	if (ret != 0 && point != NULL)
		EC_POINT_clear_free(point);
	/* Make sure to erase sensitive data */
	if (x != NULL)
		BN_clear(x);
	if (y != NULL)
		BN_clear(y);
	BN_CTX_end(bnctx);
	BN_CTX_free(bnctx);

	return (point);
}

int
ec25519_init(struct group *group)
{
	static const u_int8_t	 basepoint[CURVE25519_SIZE] = { 9 };
	struct curve25519_key	*curve25519;

	if ((curve25519 = calloc(1, sizeof(*curve25519))) == NULL)
		return (-1);

	group->curve25519 = curve25519;

	arc4random_buf(curve25519->secret, CURVE25519_SIZE);
	crypto_scalarmult_curve25519(curve25519->public,
	    curve25519->secret, basepoint);

	return (0);
}

int
ec25519_getlen(struct group *group)
{
	if (group->spec == NULL)
		return (0);
	return (CURVE25519_SIZE);
}

int
ec25519_create_exchange(struct group *group, u_int8_t *buf)
{
	struct curve25519_key	*curve25519 = group->curve25519;

	memcpy(buf, curve25519->public, ec25519_getlen(group));
	return (0);
}

int
ec25519_create_shared(struct group *group, u_int8_t *shared, u_int8_t *public)
{
	struct curve25519_key	*curve25519 = group->curve25519;

	crypto_scalarmult_curve25519(shared, curve25519->secret, public);
	return (0);
}