import os
from twilio.rest import Client

# Hello world

def lambda_handler(event, context):
    # Parse destination number and message from event
    if 'destination' in event:
        destination = event['destination']
    else:
        print("Error: 'destination' (phone number) not provided")
        return
    if 'message' in event:
        message = event['message']
    else:
        print("Error: 'message' not provided")
        return

    result = send_sms_via_twilio(destination, message)


def send_sms_via_twilio(destination, message):

    # Get Twilio credentials from environment
    account_sid = os.environ["TWILIO_ACCOUNT_SID"]
    auth_token = os.environ["TWILIO_AUTH_TOKEN"]
    from_number = os.environ["TWILIO_FROM_NUMBER"]

    # Create Twilio client
    client = Client(account_sid, auth_token)

    message = client.messages.create(
        body=f"{message}",
        from_=f"{from_number}",
        to=f"{destination}"
    )

    print(f"Sent message to '{destination}': '{message.body}'")
    return message.body

