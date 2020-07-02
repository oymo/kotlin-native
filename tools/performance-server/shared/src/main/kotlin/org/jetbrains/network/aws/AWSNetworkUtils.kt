/*
 * Copyright 2010-2019 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

package org.jetbrains.network

import kotlin.js.Promise            // TODO - migrate to multiplatform.
import kotlin.js.json               // TODO - migrate to multiplatform.
import kotlin.js.Date

@JsModule("aws-sdk")
external object AWSInstance {
    // Replace dynamic with some real type
    class Endpoint(domain: String)
    class HttpRequest(endpoint: Endpoint, region: String) {
        var method: String
        var path: String
        var body: String?
        var headers: Map<String, String>
    }
    class HttpClient() {
        val handleRequest: dynamic
    }
    class SharedIniFileCredentials(options: Map<String, String>) {
        val accessKeyId: String
    }
    class Signers() {
        class V4(request: HttpRequest, subsystem: String) {
            fun addAuthorization(credentials: SharedIniFileCredentials, date: Date)
        }
    }
}

class AWSNetworkConnector : NetworkConnector() {
    // TODO: read from config file.
    val AWSDomain = "https://vpc-kotlin-perf-service-5e6ldakkdv526ii5hbclzcmpny.eu-west-1.es.amazonaws.com"
    val AWSRegion = "eu-west-1"

    override fun <T: String?> sendBaseRequest(method: RequestMethod, path: String, user: String?, password: String?,
                                              acceptJsonContentType: Boolean, body: String?,
                                              errorHandler:(url: String, response: dynamic) -> Nothing?): Promise<T> {
        val AWSEndpoint = AWSInstance.Endpoint(AWSDomain)
        var request = AWSInstance.HttpRequest(AWSEndpoint, AWSRegion)
        request.method = method.toString()
        request.path += path
        request.body = body
        val headers = mutableMapOf<String, String>()
        headers["Host"] = AWSDomain

        if (acceptJsonContentType) {
            //headers.add("Accept" to "*/*")
            //headers["Content-Type"] = "application/json"
            //headers.add("Content-Length" to js("Buffer").byteLength(request.body))
        }
        request.headers = headers
        println(request.headers["host"])

        val credentials = AWSInstance.SharedIniFileCredentials(mapOf<String, String>())
        println(credentials.accessKeyId)
        val signer = AWSInstance.Signers.V4(request, "es")
        signer.addAuthorization(credentials, Date())

        val client = AWSInstance.HttpClient()
        client.handleRequest(request, null, { response ->
            println(response.statusCode + ' ' + response.statusMessage)
            var responseBody = ""
            response.on("data")  { chunk ->
                responseBody += chunk
                chunk
            }
            response.on("end") { chunk ->
                println("Response body: " + responseBody)
            }
        }, { error ->
            println("Error: " + error)
        })
        val util = require("util")
        val handler = util.promisify(client.handleRequest)
        return handler(request, null).then { response ->
            var responseBody = StringBuilder()
            val responseHandler = util.promisify(response.on)
            responseHandler("data").then { chunk: String ->
                responseBody.append(chunk)
            }
            responseHandler("end").then { _ ->
                responseBody.toString()
            }
        }.catch { error ->
            // Handle the error.
            errorHandler(path, error.stack)
        }
    }
}