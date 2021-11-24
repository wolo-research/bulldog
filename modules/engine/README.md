RESEARCH NOTES:

## strategy
strategy_ compression
- a standard cfr uses 16 bytes (2 * 8, regret + avg, using double)
- ?? pure strategy_ cfr uses 8 bytes (2 * 4), reg + avg, using int, both reg and avg are int only). report empirically not-worse-than performance if not better.
- ?? only the current strategy_, (1 * 4) bytes. but the last strategy_ make it more exploitable by target adapting
 (however, online average level players they dont know it is against a bot, the last strategy_ is less defensive (thus far from nash equi).
 to counter the possible neg effect, we can average over a few check point current strategy_, as used in pluribus.
- if compressing regret to 1 bit (0 - 255), by mapping a wider range of regret values into 0-255, and representing the regret value as offset value from the best action (with regret 0)
. then the memory is turn from 16 bytes to 1 byte.

## cfr solving

vector vs scaler
- regret update by (espected value vs sampled value)
- scaler more relies on discounting/flooring 
- root belief matters how vector based compuites terminal equity. scaler is not affected otherwise. 
- [ ] Vector cfr (training by estimated value. Wavg learning too fast now)
  - [ ] Opponent side pruning (reduce array size as well along the way)
  - [ ] GPU will fit for subgame solving at least, if the overall blueprint is too big for gpu. https://www.pgroup.com/products/new-features-2020.htm
  - [ ] resampling after flop | to narrow the uncovered RNB space.


## abstract game (action abs, card abs, action mapping)

game complexity
- in poker 9.17 * 10^17 game states, and 3.19 * 10 ^ 14 infosets. limited or no limited?
- addition player increase size of subgame exponentially (more choice nodes)
- m-player game can converge to 2-p or 3-p quickly. so the strategy space are actually super sparse. 