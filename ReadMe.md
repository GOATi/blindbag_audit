# GOATi 22RS Blind-bag audit tool

22RS contains "blind bag" items, which contain a pre-determined number of other, randomly chosen items.

Many games contain such systems, but often they are a "black box" that cannot be understood by users. We believe that to be fair to the users who are purchasing these kinds of items, two criteria should be met:

1. The probabilities involved in deciding the contents of each bag should be made public, so that a decision to purchase a bag is *informed*.
2. The method of evaluating those probabilities should be auditable, so that it is known to be *fair*.

Many games are beginning to adopt item number 1, above. 
We are not aware of any games that currently implement item number 2.



The types of items that are possibly found within the 22RS "blind bag", and the chances of finding each item are made public. For example: <https://22series.com/part_info?part=33554433>

We have altered our random number generation to now be **user auditable**. Neither the player, nor the game should be able to influence the roll of the dice. This has the side effect that even we do not know for sure what is inside each "bag" until it is opened by the user.

This repository contains the code to perform an audit of the random number generation.

## Background

If a game performs random number generation within the game client (on the user's computer), then it is possible for a user to cheat, by modifying their game client to not actually be random.

If a game performs random number generation on a server that is secure to the developers, cheating is avoided, but now the users can no longer be sure that the game is *fair*, i.e. that random outcomes are actually chosen randomly. Typically, this issue of trust is not a concern for traditional online games, so it is the standard solution. However, in the realm of blockchain games, the truth and fairness of a game should not be left up to trust alone, but proof of correctness.

### Statistics

One way to audit a game's random numbers is simply to collect a lot of them, and use statistical analysis to check if they conform to the probability distributions that they're supposed to. For example, if a fair coin lands on heads 90% of the time, it's probably not actually a fair coin!

In traditional games this can be almost impossible, because the game servers do not grant public access to their databases, and it's not possible to gather global samples. However, blockchain games by their nature do make this kind of auditing possible, because all decision making should be recorded on a publicly readable blockchain, rather than a private database.

We don't want to just allow statistical auditing though; actual mathematical proof would be preferred!

### VRF

One solution to the above problem is to have both the user and the game server choose a random number independently (without knowledge of the other's choice) and then add both those values together to get a new, fair random number. However, this scheme still fails to provide fair randomness in a situation where *both* parties are cheating. So to solve that, we use a **Verifiable Random Function**. Because this scheme only fails when *both* parties are cheating, we only require the game server to prove randomness via a **VRF**, and give the client leeway to provide actual random numbers or just an arbitrary choice.

#### Oracles

Trying to get two parties to each choose a random number independently (no peeking at the other's number first) is kind of like playing "Rock Paper Scissors" via mail -- If player A mails their move "Rock" to player B, how can you be sure that player B doesn't peek into the envelope before deciding their own move? One solution is to move the trust to a 3rd party - a referee. If both players mail their move to the referee, then they can attest that they received two sealed envelopes before deciding the winner. In the blockchain world, you might call this referee an "Oracle", but unless everyone fully trusts the oracle/referee, this alone doesn't solve the problem!

#### Hash functions

So instead, we rely on one-way functions -- maths that's easy to do in one direction, but practically impossible to perform in the other direction. One of the most popular of these in current use is the "SHA" hash functions. Given an input, for example, the date of the first Bitcoin block `3 January 2009` we can compute `SHA256(3 January 2009) -> fe3e5e65fb8d2a57bd4648c1b37b537ba0402e589644a0c877500a338bf7dc68`. Given the input ( `3 January 2009`) we can compute that very long hexadecimal string very quickly, but, given the result (`fe3e5e65.....`) it's practically impossible to guess what the original input was.

Going back to the example of two players playing "Rock Paper Scissors" via mail, each player can first generate the hash of their message, and send just the hash to their opponent. After they have received their opponent's hash, then they send their actual move.

For example, Alice and Bob are playing. Alice follows these steps:
1) Decide a move: "`Hi bob, today I will play scissors`"
2) Generate hash: `fab4c7cfab1310fa954f6bfa0f41efd15d55d913558fa4b780947ff1d0b49c05`
3) Send hash to Bob.
4) Receive Bob's hash: `5ffa950046bfcda9205e9ffa3a898217d81a38a41d0f161b5caf0b8c94b6760a`
5) Send move to Bob.
6) Receive Bob's move: "`I play rock, Alice!`"
7) Check that the hash bob's message matches with the hash he sent earlier.

```
SHA256("I play rock, Alice!") == 5ffa950046bfcda9205e9ffa3a898217d81a38a41d0f161b5caf0b8c94b6760a
-> true
```

If the hashes and messages match, then we know that Bob decided on his move *before* Alice sent her move to him.

#### Hash functions to make a VRF oracle

When we create one of our blind bags, we decide a random number then and there, but only publish the hash of this random number. When a blind bag owner decides to open one, they generate their own random number as part of that request. This ensures that both parties (the blind bag creator, and the blind bag opener) have chosen their random numbers independently.

Upon opening a blind bag, we reveal what our chosen random number was, and the user can verify that we did make our decision before theirs by computing the hash of the revealed number and comparing it to the hash that we published earlier. 

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
- Group #1: Item ID#5, Item ID#6
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

#### Determining seed data

Upon creation, each individual "blind bag" item is assigned several immutable properties, publicly viewable on the Phantasma blockchain.

- A cryptographically random 64bit public-seed value.
- A cryptographically random 192bit secret-seed value, encrypted using a private key that is never revealed.
- A cryptographically random 64bit salt value.
- The SHA-256 hash of the secret seed with the salt concatenated.

Upon "opening" a blind bag, the 22RS game uses the private key to decode the secret seed value, and publishes it to the Phantasma blockchain, making it publicly viewable. The previously published SHA-256 hash can be used to validate that this published secret seed is accurate.

If the secret seed alone was used to seed the random number generator, then the 22RS game would be in full control over the outcome, and to know what parts are in each bag ahead of time. To avoid this, the above-mentioned 64bit public-seed and 192bit secret-seed are combined with a 256 bit user-seed to produce the final 64bit seed for the random number generator. **This ensures that neither the 22RS game nor the user can predict the contents of the blind bag.**

To avoid giving the user complete control over their choice of seed, which would make any potential user-attacks easier to perform, the hash of their phantasma transaction that sent their "blind bag" to the 22RS game for opening is used as their 256bit seed.

*So far this entire system should be 100% cryptographically secure. The remaining steps are traditional "game logic" code that has been ported from the game, into this secure auditing environment. Cryptography experts may notice that these "game" algorithms are significantly weaker than what is expected of "secure" systems. See the Future Work section below for a roadmap of improvement.*

#### Initialization

A 64bit Xorshift algorithm is used for simplicity of implementation*. The most important detail is how the 64bit seed is calculated.

To combine the above 512-bits of total seed data, the FNV-64a hash function is used, again for simplicity of implementation*.

1. Instead of the standard FNV-64 offset basis of 0xcbf29ce484222325, the 192bit secret seed is first hashed using the 64bit public seed as the offset basis.
2. The result of this first operation is used as the offset basis when hashing the user-provided 256-bit seed.
3. The result of the second operation is used as the seed for the Xorshift algorithm.
4. The Xorshift algorithm is used as many times as required to produce random numbers used to determine the contents of the blind-bag.

This algorithm is used as a proof of concept, and should be replaced with a CSPRNG in the future to remove all possibility of unfair outcomes. Despite this, it should be fair in practice, and as fair as the previous, non-auditable implementation.

[^*]: Currently these algorithms run on a C++ server, but we wanted the ability to execute them within the PhantasmaVM, where simpler code is much cheaper to execute in terms of KCAL tokens. Also, these simple algorithms are currently in use simply because they are what we were using before we decided to make this system auditable! 

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

## Building

A project file is included for Visual Studio 2017, along with appropriate pre-built binaries of libsodium and libcurl. 

The code should be easily portable to many other C++ environments.

Pull requests are welcome.

## Limitations

We currently allow users to open more than one blind-bag per transaction, but this audit tool will only currently work on transactions where a single blind-bag was opened. This will be addressed in the future.

The GOATI NFT internal formats have changed slightly in Nov 2020 and this tool has not yet been updated for compatibility.

A rollout of an updated Phantasma NFT standard is currently scheduled for Dec 2020. Any impacts to this tool will be evaluated shortly after that rollout is completed.

## Usage

Find the Phantasma transaction hash/identifier for any transaction where a GOATi blind bag NFT has been burnt, and prizes minted into a user's wallet, then run the compiled executable with:

`blindbag_audit <txhash>`

## Collaboration

If any other game developers would like to incorporate auditable item drops, feel free to get in touch with us. We believe it's in everyone's interests to create fair play.

If any cryptographic enthusiast discover attacks or weaknesses in our approaches, please get in touch with us! 

## Thanks

Built on <https://phantasma.io/>
