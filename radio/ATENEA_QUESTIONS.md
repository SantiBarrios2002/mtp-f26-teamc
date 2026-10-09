# Questions for the Atenea forum

For Marcos to post as project leader. Draft 4-Oct-26 · Team C Radio Team.

1. **Allowed chips.** Are proprietary FSK packet transceivers allowed, specifically the HopeRF
   RFM69HCW (Semtech SX1231H)? It implements no standard wireless protocol. Is a chip also
   acceptable if it *can* do a standard PHY we never use (e.g. Si4463, which lists IEEE
   802.15.4g)?
2. **Duty cycle and the schedule.** In 869.40–869.65 MHz (CNAF UN-39) each transmitter is
   limited to 10 % of any hour (360 s). A 2-minute sending round uses ~115 s. How will the
   competition schedule SRI, MRM and NM rounds (and rehearsals) so that teams stay within that?
   Is the 10 % assessed per transmitter?
3. **2.4 GHz power.** May a non-WiFi design that uses frequency hopping or listen-before-talk
   use the 100 mW e.i.r.p. of UN-85 a), or must it stay at the 10 mW e.i.r.p. of UN-85 b)?
4. **Support height.** Is the support fixed at 70 cm, or "about 70 cm" with some tolerance?
   May the antenna sit anywhere inside the box (e.g. under the lid)?
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
