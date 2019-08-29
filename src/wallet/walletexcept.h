#ifndef BITCOINZERO_WALLET_WALLETEXCEPT_H
#define BITCOINZERO_WALLET_WALLETEXCEPT_H

#include <stdexcept>

class WalletError : public std::runtime_error
{
public:
    explicit WalletError(const char *what);
    explicit WalletError(const std::string &what);
};

class WalletLocked : public WalletError
{
public:
    WalletLocked();
};

class InsufficientFunds : public WalletError
{
public:
    InsufficientFunds();
};

#endif // BITCOINZERO_WALLET_WALLETEXCEPT_H