###############################################################################
#
# File: twilio_interface.py
#
# Author: Isaac Ingram
#
# Purpose: Provide interface for working with Twilio.
#
###############################################################################
import os
from twilio.rest import Client
import database


def send_sms(destination: str, message: str):
    """
    Send an SMS message via twilio
    :param destination: Destination phone number as a string, including country code (+1 for US) with no dashes.
    :param message: String message to send
    :return:
    """
    # Don't send message if this phone number is not registered with the application.
    if database.is_phone_number_registered(destination):
        # Get Twilio credentials from environment
        account_sid = os.environ["TWILIO_ACCOUNT_SID"]
        auth_token = os.environ["TWILIO_AUTH_TOKEN"]
        from_number = os.environ["TWILIO_FROM_NUMBER"]

        # Create Twilio client
        client = Client(account_sid, auth_token)

        message = client.messages.create(
            body=f"CS Mouse: {message}",
            from_=f"{from_number}",
            to=f"{destination}"
        )

        print(f"Sent message to '{destination}': '{message.body}'")
        return message.body
    else:
        print(f"Tried to send message to unregistered phone number '{destination}'")
