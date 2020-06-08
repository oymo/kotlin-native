/*
 * Copyright 2010-2019 JetBrains s.r.o. Use of this source code is governed by the Apache 2.0 license
 * that can be found in the LICENSE file.
 */

package org.jetbrains.network

import kotlin.js.Promise            // TODO - migrate to multiplatform.
import kotlin.js.json               // TODO - migrate to multiplatform.
import kotlin.js.Date

class AWSNetworkConnector : NetworkConnector() {
    val AWSInstance = require("aws-sdk")
    val AWSDomain = "https://vpc-kotlin-perf-service-5e6ldakkdv526ii5hbclzcmpny.eu-west-1.es.amazonaws.com"
    val AWSEndpoint = AWSInstance.Endpoint(AWSDomain)
    val AWSRegion = "eu-west-1"
    override fun <T: String?> sendBaseRequest(method: RequestMethod, path: String, user: String?, password: String?,
                                              acceptJsonContentType: Boolean, body: String?,
                                              errorHandler:(url: String, response: dynamic) -> Nothing?): Promise<T> {
        var request = AWSInstance.HttpRequest(AWSEndpoint, AWSRegion)
        request.method = method.toString()
        request.path += path
        request.body = body
        val headers = mutableListOf<Pair<String, String>>()
        headers.add("host" to AWSDomain)

        if (acceptJsonContentType) {
            headers.add("Accept" to "*/*")
            headers.add("Content-Type" to "application/json")
            headers.add("Content-Length" to js("Buffer").byteLength(request.body))
        }

        val credentials = AWSInstance.EnvironmentCredentials(password)
        val signer = AWSInstance.Signers.V4(request, "es")
        signer.addAuthorization(credentials, Date())

        val client = AWSInstance.HttpClient()
        val util = require("util")
        val handler = util.promisify(client.handleRequest)
        return handler(request, null).then { response ->
            var responseBody = StringBuilder()
            val responseHandler = util.promisify(response.on)
            responseHandler("data").then { chunk: String ->
                responseBody.append(chunk)
            }
            responseHandler("end").then { chunk ->
                responseBody.toString()
            }
        }.catch { error ->
            // Handle the error.
            errorHandler(path, error)
        }
    }
}