RANK_NAMES = [
        ('Deuce', 'Deuces'),
        ('Three', 'Threes'),
        ('Four', 'Fours'),
        ('Five', 'Fives'),
        ('Six', 'Sixes'),
        ('Seven', 'Sevens'),
        ('Eight', 'Eights'),
        ('Nine', 'Nines'),
        ('Ten', 'Tens'),
        ('Jack', 'Jacks'),
        ('Queen', 'Queens'),
        ('King', 'Kings'),
        ('Ace', 'Aces'),
]
CARDS = [r+s for r in '23456789TJQKA' for s in 'shdc']
STRING_TO_CARD = {s: i for i, s in enumerate(CARDS)}

def string_to_cards(s):
    return [STRING_TO_CARD[s[i:i+2]] for i in range(0, len(s), 2)]

def _ctz(v):
    c = 32
    v &= -v
    if v:
        c -= 1
    if v & 0x0000FFFF:
        c -= 16
    if v & 0x00FF00FF:
        c -= 8
    if v & 0x0F0F0F0F:
        c -= 4
    if v & 0x33333333:
        c -= 2
    if v & 0x55555555:
        c -= 1
    return c

def _popcount(v):
    v = v - ((v >> 1) & 0x55555555)
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333)
    return ((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24 & 63

def _from_card(card):
    return card & 3, card >> 2

def _kickers(num_kickers, rank_mask):
    num_ranks = _popcount(rank_mask)
    while num_ranks > num_kickers:
        rank_mask &= rank_mask - 1
        num_ranks -= 1
    return rank_mask

def _straight(rank_mask):
    straight = 1 if (rank_mask & 0x100f) == 0x100f else 0
    num_ranks = _popcount(rank_mask)
    while num_ranks >= 5:
        low = _ctz(rank_mask)
        if (rank_mask>>low & 0x1f) == 0x1f:
            straight = 2 + low
        rank_mask &= rank_mask - 1
        num_ranks -= 1
    return straight

def high(cards):
    rank_mask  = 0
    suit_mask  = [0]*4
    rank_count = [0]*13
    for card in cards:
        suit, rank        = _from_card(card)
        rank_mask        |= 1<<rank
        suit_mask[suit]  |= 1<<rank
        rank_count[rank] += 1

    top_arg, top_count = 0, 0
    snd_arg, snd_count = 0, 0
    for i in range(12, -1, -1):
        if rank_count[i] > top_count:
            snd_count, snd_arg = top_count, top_arg
            top_count, top_arg = rank_count[i], i
        elif rank_count[i] > snd_count:
            snd_count, snd_arg = rank_count[i], i

    for j in range(4):
        if _popcount(suit_mask[j]) >= 5:
            straight = _straight(suit_mask[j])
            if straight:
                return 8<<26 | straight, 'straight flush, %s high' % RANK_NAMES[3+straight-1][0]

            return 5<<26 | _kickers(5, suit_mask[j]), 'flush, %s high' % RANK_NAMES[_ctz(_kickers(1, suit_mask[j]))][0]

    if top_count >= 4:
        return 7<<26 | top_arg<<13 | _kickers(1, rank_mask & ~(1<<top_arg)), 'four of a kind, %s' % RANK_NAMES[top_arg][1]

    if top_count >= 3 and snd_count >= 2:
        return 6<<26 | top_arg<<13 | snd_arg, 'full house, %s and %s' % (
                RANK_NAMES[top_arg][1], RANK_NAMES[snd_arg][1]
        )

    straight = _straight(rank_mask)
    if straight:
        return 4<<26 | straight, 'straight, %s high' % RANK_NAMES[3+straight-1][0]

    if top_count >= 3:
        return 3<<26 | top_arg<<13 | _kickers(2, rank_mask & ~(1<<top_arg)), 'three of a kind, %s' % RANK_NAMES[top_arg][1]

    if top_count >= 2 and snd_count >= 2:
        return 2<<26 | (1<<top_arg | 1<<snd_arg)<<13 | _kickers(1, rank_mask & ~(1<<top_arg) & ~(1<<snd_arg)), 'two pair, %s and %s' % (
                RANK_NAMES[top_arg][1], RANK_NAMES[snd_arg][1]
        )
 
    if top_count >= 2:
        return 1<<26 | top_arg<<13 | _kickers(3, rank_mask & ~(1<<top_arg)), 'a pair of %s' % RANK_NAMES[top_arg][1]
            
    return _kickers(5, rank_mask), '%s high' % RANK_NAMES[_ctz(_kickers(1, rank_mask))][0]
