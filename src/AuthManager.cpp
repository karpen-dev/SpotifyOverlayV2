//
// Created by karpen on 10/28/25.
//

#include "../include/AuthManager.h"

#include <future>
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <iostream>
#include <cpprest/uri.h>

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class AuthManager::Impl {
public:
    http_listener listener;
    std::string authCode;
    std::promise<std::string> codePromise;
    bool codeReceived = false;

    Impl() : listener(uri_builder("http://127.0.0.1").set_port(8888).to_uri()) {
        std::cout << "AuthManager::Impl constructor started" << std::endl;

        listener.support(methods::GET, [this](http_request request) {
            handleCallback(std::move(request));
        });

        std::cout << "AuthManager::Impl constructor completed" << std::endl;
    }

    void handleCallback(const http_request &request) {
        std::cout << "Received callback request" << std::endl;

        auto query = uri::split_query(request.request_uri().query());
        const auto it = query.find("code");

        if (it != query.end() && !codeReceived) {
            authCode = it->second;
            std::cout << "Got authorization code: " << authCode.substr(0, 10) << "..." << std::endl;

            codePromise.set_value(authCode);
            codeReceived = true;

            http_response response(status_codes::OK);
            response.set_body(
                "<html><body><h1>Authentication Successful!</h1>"
                "<p>You can close this window and return to the application.</p></body></html>",
                "text/html"
            );

            request.reply(response).wait();

            std::cout << "Sent success response to browser" << std::endl;
        } else {
            std::cerr << "Missing authorization code or already received" << std::endl;
        }
    }

    void resetPromise() {
        codePromise = std::promise<std::string>();
        codeReceived = false;
        authCode.clear();
    }
};

AuthManager::AuthManager(const std::string& clientId, const std::string& clientSecret)
    : clientId_(clientId),
      clientSecret_(clientSecret),
      authCallback_(nullptr)
{
    std::cout << "AuthManager constructor started" << std::endl;
    try {
        pImpl_ = std::make_unique<Impl>();
        std::cout << "AuthManager constructor completed successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "AuthManager constructor failed: " << e.what() << std::endl;
        throw;
    }
}
AuthManager::~AuthManager() {
    stopAuthServer();
}

bool AuthManager::authenticate() {
    try {
        std::cout << "Starting authentication process..." << std::endl;

        pImpl_->resetPromise();

        startAuthServer();

        const std::string authUrl = buildAuthUrl();
        std::cout << "Opening browser with auth URL" << std::endl;

        std::string command;

        command = "xdg-open \"" + authUrl + "\"";

        int result = system(command.c_str());
        if (result != 0) {
            std::cout << "Could not open browser automatically. Please visit this URL manually:" << std::endl;
            std::cout << authUrl << std::endl;
        }

        std::cout << "Waiting for authorization code (timeout: 60 seconds)..." << std::endl;
        auto codeFuture = pImpl_->codePromise.get_future();
        const auto status = codeFuture.wait_for(std::chrono::seconds(60));

        if (status == std::future_status::timeout) {
            std::cerr << "Authentication timeout - no response within 60 seconds" << std::endl;
            stopAuthServer();
            return false;
        }

        const std::string code = codeFuture.get();
        std::cout << "Successfully received authorization code" << std::endl;
        stopAuthServer();

        return exchangeCodeForTokens(code);

    } catch (const std::exception& e) {
        std::cerr << "Authentication error: " << e.what() << std::endl;
        stopAuthServer();
        return false;
    }
}

bool AuthManager::refreshTokens(const std::string& refreshToken) {
    try {
        std::cout << "Refreshing tokens..." << std::endl;

        http_client client(U("https://accounts.spotify.com"));
        http_request request(methods::POST);

        request.set_request_uri(U("/api/token"));
        request.headers().set_content_type(U("application/x-www-form-urlencoded"));

        const std::string body = "grant_type=refresh_token&refresh_token=" + refreshToken +
                                "&client_id=" + clientId_ + "&client_secret=" + clientSecret_;

        request.set_body(body, "application/x-www-form-urlencoded");

        std::cout << "Sending refresh token request..." << std::endl;
        const auto response = client.request(request).get();

        std::cout << "Refresh response status: " << response.status_code() << std::endl;

        if (response.status_code() != status_codes::OK) {
            std::cerr << "Token refresh failed with status: " << response.status_code() << std::endl;
            return false;
        }

        auto json = response.extract_json().get();
        tokens_.accessToken = json[U("access_token")].as_string();

        if (json.has_field(U("refresh_token"))) {
            tokens_.refreshToken = json[U("refresh_token")].as_string();
        } else {
            tokens_.refreshToken = refreshToken;
        }

        tokens_.lastUpdate = std::chrono::system_clock::now();

        std::cout << "Tokens refreshed successfully" << std::endl;

        if (authCallback_) {
            authCallback_(tokens_);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Token refresh error: " << e.what() << std::endl;
        return false;
    }
}

void AuthManager::startAuthServer() const {
    try {
        std::cout << "Starting auth server..." << std::endl;
        pImpl_->listener.open().wait();
        std::cout << "Auth server started successfully on http://127.0.0.1:8888" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start auth server: " << e.what() << std::endl;
        throw;
    }
}

void AuthManager::stopAuthServer() const {
    try {
        if (pImpl_) {
            pImpl_->listener.close().wait();
            std::cout << "Auth server stopped" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error stopping auth server: " << e.what() << std::endl;
    }
}

std::string AuthManager::buildAuthUrl() const {
    const std::string encodedScope = "user-read-currently-playing%20user-modify-playback-state";
    const std::string encodedRedirect = "http%3A%2F%2F127.0.0.1%3A8888%2Fcallback";

    std::string url = "https://accounts.spotify.com/authorize?response_type=code&client_id=" + clientId_ +
                     "&scope=" + encodedScope +
                     "&redirect_uri=" + encodedRedirect;

    std::cout << "Built auth URL (scope: " << encodedScope << ")" << std::endl;
    return url;
}

bool AuthManager::exchangeCodeForTokens(const std::string& code) {
    try {
        std::cout << "Exchanging code for tokens..." << std::endl;

        http_client client(U("https://accounts.spotify.com"));
        http_request request(methods::POST);

        request.set_request_uri(U("/api/token"));
        request.headers().set_content_type(U("application/x-www-form-urlencoded"));

        const std::string encodedRedirect = "http%3A%2F%2F127.0.0.1%3A8888%2Fcallback";
        const std::string body = "grant_type=authorization_code&code=" + code +
                                "&redirect_uri=" + encodedRedirect +
                                "&client_id=" + clientId_ +
                                "&client_secret=" + clientSecret_;

        std::cout << "Sending token exchange request..." << std::endl;
        request.set_body(body, "application/x-www-form-urlencoded");

        const auto response = client.request(request).get();
        std::cout << "Token exchange response status: " << response.status_code() << std::endl;

        if (response.status_code() != status_codes::OK) {
            std::cerr << "Token exchange failed with status: " << response.status_code() << std::endl;
            return false;
        }

        auto json = response.extract_json().get();
        tokens_.accessToken = json[U("access_token")].as_string();
        tokens_.refreshToken = json[U("refresh_token")].as_string();
        tokens_.lastUpdate = std::chrono::system_clock::now();

        std::cout << "Token exchange successful!" << std::endl;

        if (authCallback_) {
            authCallback_(tokens_);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Token exchange error: " << e.what() << std::endl;
        return false;
    }
}