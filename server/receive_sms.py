###############################################################################
#
# File: receive_sms.py
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
import boto3
from twilio.request_validator import RequestValidator

dynamodb = boto3.resource('dynamodb')
registered_users_table = dynamodb.Table('cs-mouse-registered-users')

# Load twilio auth token from environment
auth_token = os.getenv("TWILIO_AUTH_TOKEN")

def is_phone_number_registered(phone_number: str) -> bool:
    """
    Get whether a phone number is registered to be used with the app.
    :param phone_number: String phone number. Includes country code (eg. +1 for US) and no dashes.
    :return: True if the item is registered, False otherwise.
    """
    response = registered_users_table.get_item(Key={"phone_number": phone_number})
    exists = response.get("Item", False)
    if exists is False:
        return False
    else:
        return True


def lambda_handler(event, context):

    try:
        # Extract headers from request
        headers = event.get("headers", {})
        twilio_signature = headers.get("x-twilio-signature", "")

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
        twilio_url = f"https://{headers.get('host', '')}{event.get('requestContext', {}).get('http', {}).get('path', '')}"

        # Validate Twilio request signature
        validator = RequestValidator(auth_token)
        if not validator.validate(twilio_url, parsed_body, twilio_signature):
            print("Invalid Twilio request")
            return {"statusCode": 403, "body": "Forbidden"}
        else:
            print("Valid Twilio request")


        # Check contents of received message to determine what action to take
        if message_body.lower() == 'register':
            # Received registration message. Check if user is already registered.
            if is_phone_number_registered(from_number):
                return {
                    "statusCode": 409,
                    "body": f"CS Mouse: You are already registered."
                }
            else:
                # Add this user to registered users
                registered_users_table.put_item(
                    Item={
                        'phone_number': from_number,
                    }
                )
                return {
                    "statusCode": 200,
                    "body": f"CS Mouse: You will now receive notifications through CS Mouse. You can stop messages at any time by replying 'STOP'."
                }

    except Exception as e:
        # Return internal server error
        return {
            "statusCode": 500
        }
    # Everything ran and no response was generated
    return {
        "statusCode": 200
    }
