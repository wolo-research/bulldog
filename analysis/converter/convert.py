import argparse, collections, datetime, itertools, os, rank, re, sys

HandHistory = collections.namedtuple(
        'HandHistory',
        ['index', 'betting', 'hands', 'board', 'outcome', 'players', 'time', 'stacks', 'blinds'],
)

def parse_HandHistory(line, freeze_out=False, stack_size=20000, blinds=[50, 100]):
    if len(line) == 0 or line[0] == '#':
        return None

    comment_start = line.find('#')
    if comment_start >= 0:
        comment = line[comment_start+1:].strip()
        line = line[0:comment_start-1]
    else:
        comment = ''

    parts = line.strip().split("|info|")
    line = parts[1]
    line = line.replace(":10000|10000", "")


    fields = line.strip().split(':')
    if len(fields) == 0:
        raise RuntimeError('Incorrect number of fields')

    if fields[0] == 'SCORE':
        return None
    elif fields[0] != 'STATE':
        raise RuntimeError('STATE field invalid')

    offset = 2 if freeze_out else 0
    num_fields = 6 + offset

    if len(fields) != num_fields:
        raise RuntimeError('Incorrect number of fields')

    if freeze_out:
        stacks = [int(x) for x in fields[2].split('|')]
        blinds = [int(x) for x in fields[3].split('|')]
        blinds = [blinds[1], blinds[0]]
    else:
        stacks = [stack_size, stack_size]

    index = int(fields[1])
    betting = fields[2+offset].split('/')
    if fields[3+offset].find('/') >= 0:
        hands, board = fields[3+offset].split('/', 1)
        hands = hands.split('|')
        board = board.split('/')
    else:
        hands = fields[3+offset].split('|')
        board = []
    outcome = map(float, fields[4+offset].split('|'))
    players = fields[5+offset].split('|')

    def cards(s):
        return [s[i:i+2] for i in range(0, len(s), 2)]
    hands = [cards(x) for x in hands]
    board = [cards(x) for x in board]

    time = None
    if comment != '':
        try:
            time = datetime.datetime.fromtimestamp(float(comment))
        except Exception as e:
            pass

    return HandHistory(index, betting, hands, board, outcome, players, time, stacks, blinds)


def convert_to_et(uct_datetime):
        # from (http://stackoverflow.com/questions/11710469/how-to-get-python-to-display-current-time-eastern)
        t = uct_datetime + datetime.timedelta(hours=-5)
        # daylight savings starts on 2nd Sunday of March
        d = datetime.datetime(t.year, 3, 8)
        dston = d + datetime.timedelta(days=6-d.weekday())
        # daylight savings stops on 1st Sunday of Nov
        d = datetime.datetime(t.year, 11, 1)
        dstoff = d + datetime.timedelta(days=6-d.weekday())
        if dston <= t.replace(tzinfo=None) < dstoff:
            t += datetime.timedelta(hours=1)
        return t


def main():
    parser = argparse.ArgumentParser(description='convert ACPC hand histories to PokerStars format')
    parser.add_argument(
            '--big_blind', type=int, default=100,
            help='Big blind'
    )
    parser.add_argument(
            '--freeze-out', dest='freeze_out', default=False, action='store_true',
            help='Indicate that logs include stack sizes'
    )
    parser.add_argument(
            '--hand_time', type=str, default=None,
            help='Time hands took place'
    )
    parser.add_argument(
            '--hand_time_delta', type=float, default=60.0,
            help='Number of seconds to add between hands'
    )
    parser.add_argument(
            '--hand_time_offset', type=float, default=0.0,
            help='Number of hours to add to hand time.  Useful when dealer\'s clock is not set in UCT.  Does not have to be interal.'
    )
    parser.add_argument(
            '--hero', type=str, default=None,
            help='Name of Hero'
    )
    parser.add_argument(
            '--log_file', type=str, default=None,
            help='Path to log file, if not set uses stdin'
    )
    parser.add_argument(
            '--player_map', type=str, default=[], nargs='+',
            help='Map player names (e.g., hyperborian_2pn_2014=Hyperborean)'
    )
    parser.add_argument(
            '--small_blind', type=int, default=50,
            help='Small blind'
    )
    parser.add_argument(
            '--stack_size', type=int, default=20000,
            help='Stack size'
    )
    parser.add_argument(
            '--table_name', type=str, default='ACPC Match',
            help='PokerStars table name'
    )
    parser.add_argument(
            '--tournament-name', dest='tournament_name', default=None,
            help='Tournament name'
    )
    parser.add_argument(
            '--tournament-id', dest='tournament_id', type=int, default=1,
            help='Tournament id'
    )
    parser.add_argument(
            '--start_index', type=int, default=1,
            help='Index of first hand'
    )
    parser.set_defaults(unix=False)
    args = parser.parse_args()

    player_map = dict([x.split('=') for x in args.player_map])
    args.hero = player_map.get(args.hero, args.hero)

    if args.hand_time:
        try:
            hand_time = datetime.datetime.strptime(args.hand_time, '%Y/%m/%d %H:%M:%S')
        except Exception:
            hand_time = args.hand_time
    elif args.log_file:
        hand_time = datetime.datetime.fromtimestamp(os.stat(args.log_file).st_mtime)
    else:
        hand_time = convert_to_et(datetime.datetime.utcnow())

    hand_time_delta = datetime.timedelta(seconds=args.hand_time_delta)
    hand_time_offset = datetime.timedelta(hours=args.hand_time_offset)

    if args.log_file:
        log_file = open(args.log_file, 'r')
    else:
        log_file = sys.stdin

    if args.tournament_name is not None:
        tournament = 'Tournament #%d, %s' % (args.tournament_id, args.tournament_name)
    else:
        tournament = ''

    index = args.start_index
    for line in log_file.xreadlines():
        hand = parse_HandHistory(line, freeze_out=args.freeze_out, stack_size=args.stack_size, blinds=[args.small_blind, args.big_blind])
        if hand:
            if hand.time is not None:
                hand_time = convert_to_et(hand.time)

            if isinstance(hand_time, datetime.datetime):
                hand_time_str = (hand_time + hand_time_offset).strftime('%Y/%m/%d %H:%M:%S ET')
                hand_time += hand_time_delta
            else:
                hand_time_str = hand_time

            hand_players = [player_map.get(x, x) for x in hand.players]
            if index == args.start_index:
                players = hand_players

            if players[0] == hand_players[0] and players[1] == hand_players[1]:
                dealer = 1
            elif players[0] == hand_players[1] and players[1] == hand_players[0]:
                dealer = 0
            else:
                players = hand_players
                dealer = 1

            if hand_players[0] == args.hero:
                hero = 0
            elif hand_players[1] == args.hero:
                hero = 1
            else:
                hero = None

            if index > args.start_index:
                print

            # add in time
            print 'PokerStars Hand #%d: %s Hold\'em No Limit ($%d/$%d USD) - %s' % (
                    index, tournament, hand.blinds[0], hand.blinds[1], hand_time_str
            )

            print 'Table \'%s\' 2-max Seat #%d is the button' % (
                    args.table_name,
                    1 + dealer
            )

            pot = [
                min(hand.blinds[0], hand.stacks[dealer]),
                min(hand.blinds[1], hand.stacks[1 - dealer]),
            ]

            print 'Seat 1: %s ($%d in chips)' % (players[0], hand.stacks[1-dealer])
            print 'Seat 2: %s ($%d in chips)' % (players[1], hand.stacks[dealer])

            sb_all_in = hand.stacks[dealer] <= hand.blinds[0]
            print '%s: posts small blind $%d%s' % (
                hand_players[0], pot[0],
                ' and is all-in' if sb_all_in else ''
            )

            bb_all_in = hand.stacks[1-dealer] <= hand.blinds[1]
            print '%s: posts big blind $%d%s' % (
                hand_players[1], pot[1],
                ' and is all-in' if bb_all_in else ''
            )

            if min(hand.stacks[0], hand.stacks[1]) <= hand.blinds[0]:
                if hand.stacks[dealer] < hand.stacks[1-dealer]:
                    print 'Uncalled bet ($%d) returned to %s' % (
                            min(hand.blinds[1], hand.stacks[1-dealer]) - min(hand.stacks[0], hand.stacks[1]),
                            hand_players[1-dealer]
                    )
                    pot[1-dealer] = hand.stacks[dealer]
                elif hand.stacks[1-dealer] < hand.stacks[dealer]:
                    print 'Uncalled bet ($%d) returned to %s' % (
                            min(hand.blinds[0], hand.stacks[dealer]) - min(hand.stacks[0], hand.stacks[1]),
                            hand_players[dealer]
                    )
                    pot[dealer] = hand.stacks[1-dealer]
                hand.betting[0] = ''
                caller = 1

            print '*** HOLE CARDS ***'
            if hero is not None:
                print 'Dealt to %s [%s %s]' % (
                        hand_players[hero],
                        hand.hands[hero][0], hand.hands[hero][1],
                )
            else:
                print 'Dealt to %s [%s %s]' % (
                        hand_players[1-dealer],
                        hand.hands[1-dealer][0], hand.hands[1-dealer][1],
                )
                print 'Dealt to %s [%s %s]' % (
                        hand_players[dealer],
                        hand.hands[dealer][0], hand.hands[dealer][1],
                )

            def summary(name1, outcome1, name2, outcome2, pot):
                if name2 == players[0]:
                    name1, name2 = name2, name1
                    outcome1, outcome2 = outcome2, outcome1
                print '*** SUMMARY ***'
                print 'Total pot $%d | Rake $0' % pot

                if len(hand.board) > 0:
                    print 'Board [%s]' % ' '.join(list(itertools.chain(*hand.board)))
                print 'Seat 1: %s %s %s' % (
                        players[0],
                        '(button) (small blind)' if dealer == 0 else '(big blind)',
                        outcome1
                )
                print 'Seat 2: %s %s %s' % (
                        players[1],
                        '(button) (small blind)' if dealer == 1 else '(big blind)',
                        outcome2
                )

            def showdown():
                player = caller
                opponent = 1 - caller

                print '*** SHOW DOWN ***'

                board_cards = list(itertools.chain(*hand.board))
                opponent_rank, opponent_rank_name = rank.high(
                        rank.string_to_cards(
                            ''.join(hand.hands[opponent]+board_cards)
                        )
                )
                print '%s: shows [%s %s] (%s)' % (
                        hand_players[opponent],
                        hand.hands[opponent][0], hand.hands[opponent][1],
                        opponent_rank_name
                )

                player_rank, player_rank_name = rank.high(
                        rank.string_to_cards(
                            ''.join(hand.hands[player]+board_cards)
                        )
                )
                print '%s: shows [%s %s] (%s)' % (
                        hand_players[player],
                        hand.hands[player][0], hand.hands[player][1],
                        player_rank_name
                )

                win_amount = pot[player] + pot[opponent]
                outcome = win_amount
                if opponent_rank == player_rank:
                    win_amount = win_amount / 2

                if opponent_rank >= player_rank:
                    print '%s collected $%d from pot' % (
                            hand_players[opponent],
                            win_amount
                    )
                if player_rank >= opponent_rank:
                    print '%s collected $%d from pot' % (
                            hand_players[player],
                            win_amount
                    )

                player_outcome = 'showed [%s %s] and %s with %s' % (
                        hand.hands[player][0], hand.hands[player][1],
                        'won ($%d)' % win_amount if player_rank >= opponent_rank else 'lost',
                        player_rank_name
                )
                opponent_outcome = 'showed [%s %s] and %s with %s' % (
                        hand.hands[opponent][0], hand.hands[opponent][1],
                        'won ($%d)' % win_amount if opponent_rank >= player_rank else 'lost',
                        opponent_rank_name
                )
                summary(hand_players[player], player_outcome,
                    hand_players[opponent], opponent_outcome,
                    outcome)

            for rnd in range(1+len(hand.board)):
                if rnd > 0:
                    print '*** %s **** [%s]' % (
                            ['FLOP', 'TURN', 'RIVER'][rnd - 1],
                            ' '.join(hand.board[rnd-1])
                    )

                player = 0 if rnd == 0 else 1
                betting = hand.betting[rnd] if rnd < len(hand.betting) else ''
                first_action = True
                initial_pot = pot[1-player] if rnd > 0 else 0

                while betting != '':
                    opponent = 1 - player

                    if betting[0] == 'f':
                        betting = betting[1:]
                        print '%s: folds' % (hand_players[player])
                        if pot[player] < pot[opponent]:
                            print 'Uncalled bet ($%d) returned to %s' % (
                                    pot[opponent] - pot[player],
                                    hand_players[opponent]
                            )
                            outcome = 2*pot[player]
                        else:
                            outcome = pot[player] + pot[opponent]
                        print '%s collected $%d from the pot' % (
                                hand_players[opponent],
                                outcome
                        )

                        folder = 'folded %s' % ['before the Flop',
                                'on the Flop',
                                'on the Turn',
                                'on the River'][rnd]
                        winner = 'collected ($%d)' % outcome
                        summary(hand_players[player], folder,
                                hand_players[opponent], winner,
                                outcome)

                    elif betting[0] == 'c':
                        betting = betting[1:]
                        if pot[player] == pot[opponent]:
                            print '%s: checks' % (hand_players[player])
                        else:
                            print '%s: calls $%d%s' % (
                                    hand_players[player],
                                    min(pot[opponent] - pot[player], hand.stacks[player] - pot[player]),
                                    ' and is all-in' if pot[opponent] >= hand.stacks[player] else ''
                            )
                            if pot[opponent] > hand.stacks[player]:
                                print 'Uncalled bet ($%d) returned to %s' % (
                                        pot[opponent] - hand.stacks[player],
                                        hand_players[opponent]
                                )
                                pot[opponent] = hand.stacks[player]
                        pot[player] = min(hand.stacks[player], pot[opponent])
                        caller = player

                    elif betting[0] == 'r':
                        betting = betting[1:]
                        size = re.match(r'\d+', betting).group(0)
                        betting = betting[len(size):]
                        size = int(size)

                        if pot[player] == initial_pot and first_action:
                            print '%s: bets $%d%s' % (
                                    hand_players[player],
                                    size - initial_pot,
                                    ' and is all-in' if size == hand.stacks[player] else ''
                            )
                        else:
                            print '%s: raises $%d to $%d%s' % (
                                    hand_players[player],
                                    size - pot[opponent], size - initial_pot,
                                    ' and is all-in' if size == hand.stacks[player] else ''
                            )
                        pot[player] = size
                    else:
                        assert False

                    player = (1 + player) % 2
                    first_action = False

            if len(hand.betting) == 4 and (hand.betting[3] == '' or hand.betting[3][-1] == 'c'):
                showdown()
            index += 1

if __name__ == '__main__':
    main()
