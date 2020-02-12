# GOATi 22RS Blind-bag audit tool

22RS contains "blind bag" items, which contain a pre-determined number of other, randomly chosen items.

Many games contain such systems, but often they are a "black box" that cannot be understood by users. We believe that to be fair to the users who are purchasing these kinds of items, two criteria should be met:

1. The probabilities involved in deciding the contents of each bag should be made public, so that a decision to purchase a bag is *informed*.
2. The method of evaluating those probabilities should be auditable, so that it is known to be *fair*.

Many games are beginning to adopt item number 1, above. 
We are not aware of **any** games that currently implement item number 2.



The types of items that are possibly found within the 22RS "blind bag", and the chances of finding each item are made public. For example: <https://22series.com/part_info?part=33554433>

We have altered our random number generation to now be **user auditable**. Neither the player, nor the game should be able to influence the roll of the dice. This has the side effect that even we do not know for sure what is inside each "bag" until it is opened by the user.

This repository contains the code to perform an audit of the random number generation.

## Description by example

An example "Pack" item type format from 22RS is described in terms of item groups, and a number of weighted "rolls":

```json
{
	"groups": [
		[1, 2, 3, 4],
		[5, 6],
		[]
	],
	"rolls": [
		[60, 35, 5],
		[120, 70, 10]
	]
}
```

The above JSON data describes:

- Group #0: Item ID#1, Item ID#2, Item ID#3, Item ID#4
- Group #1: Item ID#1, Item ID#2, Item ID#3, Item ID#4
- Group #2: *Nothing* - no item
- Roll #0: 
  - 60% chance of an item from Group #0
  - 35% chance of an item from Group #1
  - 5% chance of an item from Group #2
- Roll #1 -- same odds as above, but expressed differently
  - 120/200 chance of an item from Group #0
  - 70/200 chance of an item from Group #1
  - 10/200 chance of an item from Group #2



When this pack is opened by the user, each roll is evaluated in order.

- Roll #0 has a total weight of 100, so a random number in the range [0,99] is chosen.
  - If it is less than 60, Group#0 is used
  - Else, if it is less than 60+35, Group#1 is used
  - Else, if it is less than 60+35+5, Group#2 is used
- Roll #1 has a total weight of 200, so a random number in the range [0,199] is chosen.
  - If it is less than 120, Group#0 is used
  - Else, if it is less than 120+70, Group#1 is used
  - Else, if it is less than 120+70+10, Group#2 is used
- For each roll, another random number is generated to select an item from that group.
  - Group#0 has 4 items, so a random number in the range [0,3] is chosen.
  - Group#1 has 2 items, so a random number in the range [0,1] is chosen.
  - Group#2 has 0 items, so no random number is chosen and no item is given to the user.

### Example real data

http://www.22series.com/api/store/part_info?id=33554433&depth=0

## Random number generation

A 64bit Xorshift algorithm is used for simplicity of implementation*. The most important detail is how the 64bit seed is calculated.

Upon creation, each individual "blind bag" item is assigned several immutable properties, publicly viewable on the Phantasma blockchain.

- A random 64bit public seed value.
- A random 192bit secret seed value, encrypted using a private key that is never revealed.
- A random 64bit salt value.
- The SHA-256 hash of the secret seed with the salt concatenated.

Upon "opening" a blind bag, the 22RS game uses the private key to decode the secret seed value, and publishes it to the Phantasma blockchain, making it publicly viewable. The previously published SHA-256 hash can be used to validate that secret seed is accurate.

If the secret seed alone was used to seed the random number generator, then the 22RS game would be in full control over the outcome, and to know what parts are in each bag ahead of time. To avoid this, the above-mentioned 64bit public seed, 192bit secret seed and a 256 bit seed provided by the user are mixed together to produce the final 64bit seed for the random number generator. This ensures that neither the 22RS game nor the user can predict the contents of the blind bag.

To avoid giving the user complete control over their choice of seed, which would make user-attacks easier to perform, the hash of their phantasma transaction that sent their "blind bag" to the 22RS game for opening is used as their 256bit seed.

To combine these seed values, the FNV-64a hash function is used, again for simplicity of implementation*.

1. Instead of the standard FNV-64 offset basis of 0xcbf29ce484222325, the 192bit secret seed is first hashed using the 64bit public seed as the offset basis.
2. The result of this first operation is used as the offset basis when hashing the user-provided 256-bit seed.
3. The result of the second operation is used as the seed for the Xorshift algorithm.

This algorithm is used as a proof of concept, and should be replaced with a CSPRNG in the future to remove all possibility of cheating. See the Future Work section below for a roadmap of improvement.

[^*]: Currently these algorithms run on a C++ server, but we may want to port them to the PhantasmaVM, where simpler code is much cheaper to execute in terms of KCAL tokens. Also, these simple algorithms are currently in use simply because they are what we were using before we decided to make this system auditable! 

## Implementation

[hash.h](hash.h) - FNV64a implementation

[random.h](random.h) - Xorshift64M implementation

[goati_nft.h](goati_nft.h) - Decoding of Phantasma NFT ROM and RAM data

[Audit.h](Audit.h) - Implementation of the "rolls" and "groups" blind-bag opening algorithm, along with auditing code to compare against the values published to the blockchain.

[main.cpp](main.cpp) - Command line tool to audit a particular blockchain transaction.

## Dependencies

Included in this repository: C++ Phantasma SDK, libcurl, libsodium

## Future work

Neither the Xorshift64M or the FNV64a algorithms have any cryptographic properties, and may be able to be subject to bias, bad distribution properties, or attacks by a determined user. These should be "fair enough" for production use in a game (they are the same algorithms we would traditionally use within game development) but have weaknesses that are out of place in a cryptographic system.

User attacks to control the items generated require the user to be able to precisely alter their phantasma transaction payloads so that the generated double-SHA-256 transaction hash contains the specific values that they require to influence the FNV hash. This would be similar in complexity to bitcoin mining, which is hopefully a more profitable activity for these attackers to undertake ;) 

There may also be bias present in these algorithms that causes the "random" numbers to not be quite as uniform as we intend -- causing some items to be given to users more often than others, even though the weights are equal. If such bias is present, it could be exploited by the 22RS game to subtly influence the items discovered by users. This would represent an *unfairness* in the system that undermines our entire mission.

Another auditing tool is in development to perform a statistical analysis of the behaviour of the RNG over a period of time (by looking at all transactions on the blockchain) to identify any bias that is occurring in practice. 

In the future we will migrate to cryptographic hashing and CSPRNG algorithms in these sections of the implementation, to provide full confidence in the fairness of the system.