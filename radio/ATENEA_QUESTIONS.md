# Questions for the Atenea forum

For Marcos to post as project leader. Draft 4-Oct-26 · Team C Radio Team.

1. **Allowed chips.** Are proprietary GFSK packet transceivers allowed, specifically the Nordic
   nRF24L01+ on the Ebyte E01-ML01DP5 module (with a PA/LNA)? Its Enhanced ShockBurst packet
   format is Nordic-proprietary, not a standard wireless protocol, and it has no Bluetooth.
2. *(Obsolete 9-Oct-26: all teams moved to 2.4 GHz, which has no duty-cycle limit. Do not post.)*
   **Duty cycle and the schedule.** In 869.40–869.65 MHz (CNAF UN-39) each transmitter is
   limited to 10 % of any hour (360 s). A 2-minute sending round uses ~115 s. How will the
   competition schedule SRI, MRM and NM rounds (and rehearsals) so that teams stay within that?
   Is the 10 % assessed per transmitter?
3. **2.4 GHz power (now the most valuable question: +10 dB on the 260 m link).** All teams now use
   2.4 GHz. If our nRF24-based radio hops over ≥ 15 channels (FHSS in the sense of EN 300 328) with
   listen-before-talk, may it use the 100 mW e.i.r.p. of CNAF UN-85 a)? Or must every team stay at
   the 10 mW e.i.r.p. of UN-85 b)? (A non-hopping narrowband radio is held to 10 mW/MHz under UN-85 a)
   anyway.) Also: is the Ebyte E01-ML01DP5 (Nordic nRF24L01P, proprietary Enhanced ShockBurst) fine
   under the standard-protocol ban (see Q1)?
4. **Support height.** Is the support fixed at 70 cm, or "about 70 cm" with some tolerance?
   May the antenna sit anywhere inside the box (e.g. under the lid)? May the box stand on its
   15 × 7 cm edge on the support (so a vertical antenna board can be taller)?
5. **Cooperation.** Is it acceptable for teams to agree a common NM physical layer and an MRM
   frequency split? Is sharing radio settings (not code) within the rules?
6. **USB access during setup.** The rules say the containers stay wrapped in aluminium foil while
   we download the file from the USB memory device. May we open the foil locally at the USB port
   to insert the stick? And may the stick be removed before Tx/Rx starts, with the file kept in
   on-board memory?
7. **Combined boards.** Does a board that carries both the microprocessor and the radio transceiver
   (e.g. Adafruit Feather RP2040 RFM69) count as the one allowed microprocessor module, or as a
   "single commercial module embedding the entire system"?
8. **Role switch.** The professor picks the transmitting device after the file has been loaded into
   both. May each device have a TX/RX role switch? It does not indicate network position.
9. **Encoding and output name.** Is "UNICODE .txt" UTF-8 or UTF-16? And what file name should the
   received file have on the USB stick (the original name, or a fixed one)?
10. **Two antennas.** Please confirm that a device may use two antennas (we plan one patch on the
    front wall and one on the back wall, inside the box).
