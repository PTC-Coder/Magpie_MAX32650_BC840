# Magpie\_MAX32650\_BC840



Code Repository for Magpie Main board with a MAX32650 and a BC840.  The BC840 in an FCC certified nRF52840 CKAA module produced by Fanstel.

The Fanstel BC840 series of modules, including the BC840, BC840M, and BC840E, utilizes the nRF52840-CKAA variant



While many standard nRF52840 modules use the larger QFN73 (QIAA) package, the BC840 series uses the WLCSP (Wafer Level Chip Scale Package) variant to maintain its ultra-compact footprint (7.1 x 9.2 x 1.5 mm).



Key Technical DetailsSoC Variant: nRF52840-CKAAPackage Type: WLCSP (Wafer Level Chip Scale Package)

Memory: 1MB Flash / 256KB RAM

Security: Integrated ARM CryptoCell-310 co-processor.



In Zephyr, you should use the **nRF52840-QIAA** (or simply the standard nrf52840) target.



The CKAA and QIAA are functionally identical in terms of their core silicon features, memory (1MB Flash / 256KB RAM), and peripheral set. The only difference is the physical package: the CKAA is a compact Wafer Level Chip Scale Package (WLCSP) while the QIAA is a larger aQFN73 package.

