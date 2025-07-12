###############################################################################
#
# File: lambda_function.py
#
# Author: Isaac Ingram
#
# Purpose: Provide AWS Lambda function for Twilio to call when an SMS is sent
# to the app.
#
###############################################################################
import base64
import urllib.parse
import os
from twilio.request_validator import RequestValidator
from common import database as db
from common import messaging_service as ms
import json

# Load twilio auth token from environment
auth_token = os.getenv("TWILIO_AUTH_TOKEN", None)


def lambda_handler(event, context):
    print(json.dumps(event, indent=2))

    if auth_token is None:
        print("TWILIO_AUTH_TOKEN not set!")
        return {
            "statusCode": 500,
            "headers": {
                "Content-Type": "text/json"
            },
            "body": "Internal error, please see logs."
        }

    try:
        # Extract headers from request
        headers = event.get("headers", {})
        twilio_signature = headers.get("X-Twilio-Signature") or ""
        if not twilio_signature:
            print("Warning: x-twilio-signature is missing or empty!")

        # Extract body, which might be base 64 encoded
        if event.get("isBase64Encoded"):
            decoded_body = base64.b64decode(event["body"]).decode("utf-8")
        else:
            decoded_body = event.get("body", "")

        # Twilio sends data as x-www-form-urlencoded so decode it
        parsed_body = urllib.parse.parse_qs(decoded_body, keep_blank_values=True)

        # Convert lists to single values (Twilio always sends single values per key)
        parsed_body = {key: value[0] for key, value in parsed_body.items()}

        # Extra message body and from number
        message_body = parsed_body.get("Body", "")
        from_number = parsed_body.get("From", "")

        print(f"Received message from {from_number}: {message_body}")

        # Construct URL for Twilio request validation
        twilio_url = f"https://{headers.get('Host', '')}{event.get('requestContext', {}).get('path', '')}"

        print(f"Constructed URL for validation: {twilio_url}")

        # Validate Twilio request signature
        print("Doing request validator...")
        validator = RequestValidator(auth_token)
        print("Did request validator...")
        if not validator.validate(twilio_url, parsed_body, twilio_signature):
            print("Invalid Twilio request")
            return {
                "statusCode": 403,
                "headers": {
                    "Content-Type": "text/json"
                },
                "body": "Forbidden"
            }
        else:
            print("Valid Twilio request")

        # Check contents of received message to determine what action to take
        match message_body.lower():
            case 'register':
                # Received registration message. Check if user is already registered.
                if db.is_phone_number_registered(from_number):
                    print("User already registered")
                    return {
                        "statusCode": 200,
                        "headers": {
                            "Content-Type": "application/xml"
                        },
                        "body": ms.build_twiml_message_response("CS Mouse: You are already registered.")
                    }

                else:
                    print("New user registered")
                    db.register_phone_number(from_number)
                    return {
                        "statusCode": 200,
                        "headers": {
                            "Content-Type": "text/json"
                        },
                        "body": ms.build_twiml_message_response("CS Mouse: You will now receive notifications through CS Mouse. You can stop messages at any time by replying 'STOP'.")
                    }


    except Exception as e:
        # Return internal server error
        print(f"Exception caught: {e}")
        return {
            "statusCode": 500,
            "headers": {
                "Content-Type": "text/json"
            },
            "body": "Internal error, please see logs."
        }
    # Everything ran and no response was generated
    print("No response generated")
    return {
        "statusCode": 200,
        "headers": {
            "Content-Type": "text/json"
        },
        "body": "OK"
    }
