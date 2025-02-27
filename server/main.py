import os
from twilio.rest import Client
import boto3
from boto3.dynamodb.conditions import Key
from typing import List, Union

LOW_BATTERY_LEVEL = 0.1 # Battery alert level

dynamodb = boto3.resource('dynamodb')

homes_table: dynamodb.Table = dynamodb.Table('cs-mouse-homes')
residents_table: dynamodb.Table = dynamodb.Table('cs-mouse-residents')
traps_table: dynamodb.Table = dynamodb.Table('cs-mouse-traps')
traps_to_homes_table: dynamodb.Table = dynamodb.Table('cs-mouse-traps-to-homes')
homes_to_residents_table: dynamodb.Table = dynamodb.Table('cs-mouse-homes-to-residents')

def send_sms_via_twilio(destination: str, message: str):
    """
    Send an SMS message via twilio
    :param destination: Destination phone number as a string, including country code (+1 for US) with no dashes.
    :param message: String message to send
    :return:
    """

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


def get_trap_data_by_id(trap_id):
    """
    Get trap data from the database by trap ID
    :param trap_id:
    :return:
    """
    response = traps_table.get_item(Key={"trap_id": trap_id})
    return response.get("Item", None)


def update_trap(trap_id, hammer_status: Union[bool, None] = None, battery_percent: Union[float, None] = None):
    """
    Update a trap in the database
    :param trap_id: ID of the trap to update
    :param hammer_status: Optional boolean hammer status
    :param battery_percent: Optional float battery percent
    :return: Update attributes
    """
    update_expression = "SET "
    expression_attribute_values = {}
    expression_attribute_names = {}

    update_fields = []
    if hammer_status is not None:
        update_fields.append("#hd = :hammer_down")
        expression_attribute_values[":hammer_down"] = hammer_status
        expression_attribute_names["#hd"] = "hammer_down"

    if battery_percent is not None:
        update_fields.append("#bp = :battery_level")
        expression_attribute_values[":battery_level"] = battery_percent
        expression_attribute_names["#bp"] = "battery_level"

    if not update_fields:
        return

    update_expression += ", ".join(update_fields)

    response = traps_table.update_item(
        Key={"trap_id": trap_id},
        UpdateExpression=update_expression,
        ExpressionAttributeValues=expression_attribute_values,
        ExpressionAttributeNames=expression_attribute_names,
        ReturnValues="UPDATED_NEW"
    )

    return response["Attributes"]



def lambda_handler(event, context):

    messages_to_send = list()

    # Iterate through all traps data was sent for
    for trap_id in event['traps']:

        # Get JSON data for this trap
        trap_data = event['traps'][trap_id]
        hammer_down = trap_data['hammerDown']
        battery_level = trap_data['batteryLevel']

        # Get previous trap data from the database
        old_trap_data = get_trap_data_by_id(trap_id)

        if old_trap_data is not None:

            trap_name = old_trap_data.get('name', 'Unnamed')

            # Process deltas from previous data to send messages
            if hammer_down and not old_trap_data['hammer_down']:
                # Hammer has gone down
                print("Added hammer down message")
                messages_to_send.append((trap_data, f"{trap_name} trap triggered"))

            if battery_level <= LOW_BATTERY_LEVEL < old_trap_data['battery_level']:
                # Battery has fallen below low battery level
                messages_to_send.append((trap_data, f"{trap_name} battery level low"))
                pass

            # Update trap in DB with new data
            update_trap(trap_id, hammer_status=hammer_down, battery_percent=battery_level)
        else:
            # Trap doesn't exist in DB, add it
            # TODO add new trap to db
            pass


    # Iterate through all messages to send
    for trap_data, message in messages_to_send:
        # Iterate through all phone numbers for each message
        phone_numbers = trap_data.get('resident_phone_numbers', set())
        if len(phone_numbers) == 0:
            print("Phone numbers list size 0")
        for phone_number in phone_numbers:
            # Send the message
            send_sms_via_twilio(phone_number, message)

