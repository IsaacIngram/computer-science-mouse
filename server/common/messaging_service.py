###############################################################################
#
# File: messaging_service.py
#
# Author: Isaac Ingram
#
# Purpose:
#
###############################################################################
from common.database import is_phone_number_registered
from twilio.rest import Client
from os import environ
from xml.etree.ElementTree import Element, tostring


TWILIO_ACCOUNT_SID = environ.get('TWILIO_ACCOUNT_SID')
TWILIO_AUTH_TOKEN = environ.get('TWILIO_AUTH_TOKEN')
TWILIO_FROM_NUMBER = environ.get('TWILIO_FROM_NUMBER')


def build_twiml_message_response(message: str) -> str:
    response = Element("Response")
    msg = Element("Message")
    msg.text = message
    response.append(msg)
    return tostring(response, encoding="utf-8", method="xml").decode()


def send_sms(phone_number: str, message: str) -> str:
    """
    Send an SMS message via twilio
    :param phone_number: Destination phone number as a string, including country code (+1 for US) with no dashes.
    :param message: String message to send.
    :return: Message body sent,
    """
    # Don't send message if this phone number is not registered with the application.
    if is_phone_number_registered(phone_number):
        # Get Twilio credentials from environment
        # Create Twilio client
        client = Client(TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN)

        message = client.messages.create(
            body=f"CS Mouse: {message}",
            from_=f"{TWILIO_FROM_NUMBER}",
            to=f"{phone_number}"
        )

        print(f"Sent message to '{phone_number}': '{message.body}'")
        return message.body
    else:
        raise(Exception(f"Tried to send message to unregistered phone number '{phone_number}'"))
