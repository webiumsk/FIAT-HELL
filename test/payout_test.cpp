#include "ota/FirmwareVersion.h"
#include "payout/Bolt11.h"
#include "payout/NoteMask.h"
#include "payout/OfflineLnurl.h"
#include "payout/Quote.h"
#include "portal/PortalPolicy.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed = 0;

static void expect(bool cond, const char *message) {
  if (!cond) {
    fprintf(stderr, "FAIL %s\n", message);
    g_failed++;
  }
}

static void expectQuote(int64_t cents, double price, double fee, int64_t sats,
                       const char *message) {
  const Quote quote = quoteSats(cents, price, fee);
  expect(quote.ok && quote.sats == sats, message);
}

int main() {
  // (cents/100) / price * 1e8 * (100-fee) / 100, then round to sats.
  expectQuote(10000, 50000.0, 0.0, 200000, "100 EUR is 200000 sats");
  expectQuote(10000, 50000.0, 10.0, 180000, "10 percent fee");
  expectQuote(10000, 50000.0, 2.5, 195000, "fractional fee");
  expectQuote(1, 30000.0, 0.0, 33, "sub-euro amount rounds to 33 sats");
  expectQuote(1, 2000000.0, 0.0, 1, "half a sat rounds up to 1");
  expect(!quoteSats(0, 50000.0, 0.0).ok, "zero cents");
  expect(!quoteSats(-100, 50000.0, 0.0).ok, "negative cents");
  expect(!quoteSats(10000, 0.0, 0.0).ok, "zero price");
  expect(!quoteSats(10000, -1.0, 0.0).ok, "negative price");
  expect(!quoteSats(10000, NAN, 0.0).ok, "NaN price");
  expect(!quoteSats(10000, INFINITY, 0.0).ok, "infinite price");
  expect(!quoteSats(10000, 50000.0, -0.1).ok, "negative fee");
  expect(!quoteSats(10000, 50000.0, NAN).ok, "NaN fee");
  expect(!quoteSats(10000, 50000.0, 100.1).ok, "fee above 100");
  expect(!quoteSats(10000, 50000.0, 100.0).ok, "fee of 100 yields no sats");
  expect(!quoteSats(1, 100000000.0, 0.0).ok, "sub-sat amount is not payable");
  expect(quoteSats(10000, 50000.0, 0.0).ok, "zero fee is allowed");
  // 1.00 PYG at a realistic BTC price is under one sat. A real note is not.
  expect(!quoteSats(100, 800000000.0, 0.0).ok, "one PYG rounds to zero sats");
  expect(quoteSats(200000, 800000000.0, 0.0).sats == 250, "2000 PYG quotes");

  const MixedLeg legs[] = {
      {10000, 50000.0, 0.0},
      {0, 0.0, 99.0},
      {5000, 50000.0, 0.0},
  };
  const Quote mixed = quoteMixedSats(legs, 3);
  expect(mixed.ok && mixed.sats == 300000, "mixed skips an empty leg");

  const MixedLeg missingPrice[] = {{10000, 0.0, 0.0}, {5000, 50000.0, 0.0}};
  expect(!quoteMixedSats(missingPrice, 2).ok, "one missing price fails the quote");
  const MixedLeg badFee[] = {{10000, 50000.0, 0.0}, {100, 50000.0, 150.0}};
  expect(!quoteMixedSats(badFee, 2).ok, "one bad fee fails the quote");
  const MixedLeg negative[] = {{-1, 50000.0, 0.0}};
  expect(!quoteMixedSats(negative, 1).ok, "negative mixed cents");
  const MixedLeg none[] = {{0, 0.0, 0.0}, {0, 50000.0, 0.0}};
  expect(!quoteMixedSats(none, 2).ok, "no funded leg");
  expect(!quoteMixedSats(nullptr, 1).ok, "null legs");
  expect(!quoteMixedSats(legs, 0).ok, "empty leg list");

  int64_t sats = 0;
  expect(bolt11AmountSats("lightning:lnbc9740n1qqqq", &sats) && sats == 974,
         "lightning: prefix");
  expect(bolt11AmountSats("  LIGHTNING:lnbc9740n1qqqq", &sats) && sats == 974,
         "uppercase lightning prefix");
  expect(bolt11AmountSats("lnbc2500u1qqqq", &sats) && sats == 250000,
         "2500 uBTC");
  expect(bolt11AmountSats("LNBC20M1QQ", &sats) && sats == 2000000, "20 mBTC");
  expect(bolt11AmountSats("lnbc10n1qq", &sats) && sats == 1, "10 nBTC is 1 sat");
  expect(bolt11AmountSats("lnbc10000p1qq", &sats) && sats == 1, "10000 pBTC is 1 sat");
  expect(bolt11AmountSats("lntb1u1qq", &sats) && sats == 100, "testnet prefix");
  expect(bolt11AmountSats("lnbcrt1u1qq", &sats) && sats == 100, "regtest prefix");
  expect(bolt11AmountSats("lnbc1u1qq", &sats) && sats == 100, "1 uBTC");
  expect(!bolt11AmountSats("lnbc1n1qq", &sats), "fractional sat rejected");
  expect(!bolt11AmountSats("lnbc1p1qq", &sats), "pico fraction rejected");
  expect(!bolt11AmountSats("lnbc1qqqq", &sats), "amountless invoice");
  expect(!bolt11AmountSats("lnbc01u1qq", &sats), "leading zero rejected");
  expect(!bolt11AmountSats("lnbc2500.5u1qq", &sats), "decimal amount rejected");
  expect(!bolt11AmountSats("lnbc1x1qq", &sats), "unknown multiplier");
  expect(!bolt11AmountSats("not-an-invoice", &sats), "malformed invoice");
  expect(!bolt11AmountSats(nullptr, &sats), "null invoice");
  expect(!bolt11AmountSats("lnbc1u1qq", nullptr), "null output");
  expect(!bolt11AmountSats("", &sats), "empty invoice");

  const OfflinePayout single =
      offlineLnurlPayout(FundingKind::Lnbits, false, 10000, 10.0);
  const Quote sameQuote = quoteSats(10000, 50000.0, 10.0);
  expect(single.allowed && single.centsAfterFee == 9000, "offline cents after fee");
  expect(sameQuote.ok && single.centsAfterFee != (uint64_t)sameQuote.sats,
         "offline field is cents, not sats");
  expect(offlineLnurlPayout(FundingKind::Lnbits, false, 10000, 0.0).centsAfterFee ==
             10000,
         "zero fee keeps the cents");
  expect(offlineLnurlPayout(FundingKind::Lnbits, false, 101, 50.0).centsAfterFee ==
             51,
         "half a cent rounds away from zero");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, true, 10000, 0.0).allowed,
         "mixed offline refused");
  expect(!offlineLnurlPayout(FundingKind::Galoy, false, 10000, 0.0).allowed,
         "galoy offline refused");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, 10000, 100.0).allowed,
         "offline fee leaves nothing");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, 1, 99.0).allowed,
         "one cent and a 99 percent fee rounds away");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, 0, 0.0).allowed,
         "zero cents");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, -5, 0.0).allowed,
         "negative cents");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, 10000, 100.1).allowed,
         "offline fee above 100");
  expect(!offlineLnurlPayout(FundingKind::Lnbits, false, 10000, NAN).allowed,
         "offline NaN fee");

  expect(!portalPasswordAccepted(nullptr), "null password");
  expect(!portalPasswordAccepted("changeme"), "default password refused");
  expect(!portalPasswordAccepted(""), "empty password refused");
  expect(!portalPasswordAccepted("short"), "short password refused");
  expect(!portalPasswordAccepted("1234567"), "seven characters refused");
  expect(portalPasswordAccepted("12345678"), "eight characters accepted");
  expect(portalPasswordAccepted("correct-horse"), "long password accepted");
  expect(portalPasswordAccepted("changeme!"), "default plus a character is accepted");
  char longPassword[80];
  memset(longPassword, 'a', 63);
  longPassword[63] = '\0';
  expect(portalPasswordAccepted(longPassword), "63 characters accepted");
  longPassword[63] = 'a';
  longPassword[64] = '\0';
  expect(!portalPasswordAccepted(longPassword), "64 characters refused");

  expect(portalPathIsSensitive("/config"), "config");
  expect(portalPathIsSensitive("/save"), "save");
  expect(portalPathIsSensitive("/savefirst"), "save first is its own path");
  expect(portalPathIsSensitive("/first"), "first");
  expect(portalPathIsSensitive("/second"), "second");
  expect(portalPathIsSensitive("/savesecond"), "save second");
  expect(portalPathIsSensitive("/third"), "third");
  expect(portalPathIsSensitive("/savethird"), "save third");
  expect(portalPathIsSensitive("/gui"), "gui");
  expect(portalPathIsSensitive("/savegui"), "save gui");
  expect(portalPathIsSensitive("/_ac"), "autoconnect root");
  expect(portalPathIsSensitive("/_ac/config"), "autoconnect subpage");
  expect(portalPathIsSensitive("/flashkey"), "flashkey");
  expect(portalPathIsSensitive("/flashkey/run"), "flashkey run");
  expect(portalPathIsSensitive("/ota"), "ota");
  expect(portalPathIsSensitive("/ota_do"), "ota action");
  expect(!portalPathIsSensitive("/"), "customer home is not a settings path");
  expect(!portalPathIsSensitive("/saveX"), "save is not a prefix");
  expect(!portalPathIsSensitive("/ot"), "ota prefix is four characters");
  expect(!portalPathIsSensitive("/flash"), "flashkey prefix is the whole word");
  expect(!portalPathIsSensitive(nullptr), "null path");

  expect(firmwareUpdateRejected("fiat-hell-v1.2.0.bin", "1.3.3"), "downgrade");
  expect(firmwareUpdateRejected("fiat-hell-v1.3.2.bin", "1.3.3"), "older patch");
  expect(firmwareUpdateRejected("fiat-hell-v1.9.9.bin", "1.10.0"),
         "numeric minor, not a string compare");
  expect(!firmwareUpdateRejected("fiat-hell-v1.3.3.bin", "1.3.3"), "same version");
  expect(!firmwareUpdateRejected("fiat-hell-v1.4.0.bin", "1.3.3"), "upgrade");
  expect(!firmwareUpdateRejected("fiat-hell-v1.3.10.bin", "1.3.9"), "newer patch");
  expect(!firmwareUpdateRejected("fiat-hell-v1.10.0.bin", "1.9.0"), "newer minor");
  expect(!firmwareUpdateRejected("fiat-hell-V1.4.0.bin", "1.3.3"), "uppercase V");
  expect(firmwareUpdateRejected("firmware.bin", "1.3.3"), "missing version");
  expect(firmwareUpdateRejected("fiat-hell-v1.3.bin", "1.3.3"), "missing patch");
  expect(firmwareUpdateRejected(nullptr, "1.3.3"), "null filename");
  expect(firmwareUpdateRejected("fiat-hell-v1.4.0.bin", nullptr), "null current");
  expect(firmwareUpdateRejected("fiat-hell-v1.4.0.bin", "1.3"), "current missing patch");

  // The version marker embedded in the image itself.
  uint8_t image[512];
  memset(image, 0, sizeof(image));
  memcpy(image + 100, "FHFW:1.3.3", 10);
  expect(!firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "image marker same version");
  memcpy(image + 100, "FHFW:1.2.9", 10);
  expect(firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "image marker older");
  memcpy(image + 100, "FHFW:1.10.0", 11);
  expect(!firmwareImageDowngrade(image, sizeof(image), "1.9.0"),
         "image marker newer minor is numeric");
  memset(image, 0, sizeof(image));
  expect(firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "image without marker rejected");
  memcpy(image + 100, "FHFW:1.3", 8);
  expect(firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "image marker without patch rejected");
  memcpy(image + 100, "FHFW:1.4.0", 10);
  expect(firmwareImageDowngrade(image, sizeof(image), nullptr),
         "null current rejected");
  expect(firmwareImageDowngrade(nullptr, sizeof(image), "1.3.3"),
         "null image rejected");
  uint8_t tiny[8] = {0};
  expect(firmwareImageDowngrade(tiny, sizeof(tiny), "1.3.3"),
         "truncated image rejected");
  // The bare needle is linked into the image too; it must not hide the
  // real marker regardless of which the linker places first.
  memset(image, 0, sizeof(image));
  memcpy(image + 50, "FHFW:", 6);
  memcpy(image + 200, "FHFW:1.4.0", 10);
  expect(!firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "needle before the marker is skipped");
  memset(image, 0, sizeof(image));
  memcpy(image + 50, "FHFW:", 6);
  expect(firmwareImageDowngrade(image, sizeof(image), "1.3.3"),
         "needle alone is not a marker");
  uint8_t tail[] = {'F', 'H', 'F', 'W', ':', '1', '.', '3', '.', '3'};
  expect(!firmwareImageDowngrade(tail, sizeof(tail), "1.3.3"),
         "marker at the very end of the image");

  // Wallet balance in sats for each funding unit.
  int64_t walletSats = 0;
  expect(walletBalanceSats(BalanceUnit::Sats, 5000, 0.0, &walletSats) &&
             walletSats == 5000,
         "sats balance is used as is");
  expect(walletBalanceSats(BalanceUnit::Msats, 5999, 0.0, &walletSats) &&
             walletSats == 5,
         "msats round down");
  expect(!walletBalanceSats(BalanceUnit::Msats, 999, 0.0, &walletSats),
         "under one sat is not a balance");
  expect(walletBalanceSats(BalanceUnit::UsdCents, 5000, 50000.0, &walletSats) &&
             walletSats == 100000,
         "50 USD at 50000 USD/BTC is 100000 sats");
  expect(!walletBalanceSats(BalanceUnit::UsdCents, 5000, 0.0, &walletSats),
         "USD balance without a USD price");
  expect(!walletBalanceSats(BalanceUnit::UsdCents, 5000, NAN, &walletSats),
         "USD balance with a NaN price");
  expect(!walletBalanceSats(BalanceUnit::Sats, 0, 0.0, &walletSats),
         "empty balance");
  expect(!walletBalanceSats(BalanceUnit::Sats, -5, 0.0, &walletSats),
         "negative balance");
  expect(!walletBalanceSats(BalanceUnit::Sats, 5, 0.0, nullptr),
         "null output");

  // Which channels stay open. Leg 0 is 50000 per BTC: 5 EUR = 10000 sats,
  // 10 EUR = 20000, 50 EUR = 100000. Leg 1 is 2000000 per BTC: 1000 units =
  // 50000 sats.
  const NoteChannel channels[] = {{0, 5}, {0, 10}, {0, 50}, {1, 1000}};
  const MixedLeg empty[3] = {
      {0, 50000.0, 0.0}, {0, 2000000.0, 0.0}, {0, 0.0, 0.0}};
  expect(fittingNoteMask(channels, 4, empty, 20000) == 0x0003,
         "20000 sats covers the 5 and 10 EUR notes only");
  expect(fittingNoteMask(channels, 4, empty, 4000) == 0x0000,
         "4000 sats covers no channel");
  expect(fittingNoteMask(channels, 4, empty, 100000) == 0x000F,
         "100000 sats covers every channel");
  expect(fittingNoteMask(channels, 4, empty, 50000) == 0x000B,
         "50000 sats covers everything except the 50 EUR note");

  // 5 EUR already inserted (500 cents = 10000 sats).
  const MixedLeg partial[3] = {
      {500, 50000.0, 0.0}, {0, 2000000.0, 0.0}, {0, 0.0, 0.0}};
  expect(fittingNoteMask(channels, 4, partial, 20000) == 0x0001,
         "inserted cash counts against the wallet");
  expect(fittingNoteMask(channels, 4, partial, 10000) == 0x0000,
         "wallet already used up by the inserted cash");

  const NoteChannel noPriceChannel[] = {{2, 5}};
  expect(fittingNoteMask(noPriceChannel, 1, empty, 1000000) == 0,
         "a currency without a price stays closed");
  const MixedLeg badFeeLegs[3] = {
      {0, 50000.0, 150.0}, {0, 2000000.0, 0.0}, {0, 0.0, 0.0}};
  expect(fittingNoteMask(channels, 4, badFeeLegs, 1000000) == 0x0008,
         "a leg with a bad fee stays closed, other legs stay open");
  const NoteChannel badLeg[] = {{3, 5}, {-1, 5}, {0, 0}, {0, -5}};
  expect(fittingNoteMask(badLeg, 4, empty, 1000000) == 0,
         "bad leg index or amount stays closed");
  expect(fittingNoteMask(channels, 4, empty, 0) == 0, "no wallet, no notes");
  expect(fittingNoteMask(nullptr, 4, empty, 1000) == 0, "null channels");
  expect(fittingNoteMask(channels, 4, nullptr, 1000) == 0, "null legs");
  NoteChannel many[20];
  for (int i = 0; i < 20; i++) {
    many[i] = {0, 5};
  }
  expect(fittingNoteMask(many, 20, empty, 1000000) == 0xFFFF,
         "only the first 16 channels are considered");

  if (g_failed != 0) {
    fprintf(stderr, "%d assertion(s) failed\n", g_failed);
    return 1;
  }
  printf("payout tests ok\n");
  return 0;
}
