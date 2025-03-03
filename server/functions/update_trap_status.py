###############################################################################
#
# File: update_trap_satus.py
#
# Author: Isaac Ingram
#
# Purpose: Provide function for AWS Lambda to handle trap status updates.
#
###############################################################################
from lib import twilio_interface, database

LOW_BATTERY_LEVEL = 10 # Battery alert level


def lambda_handler(event, context):
    """
    Lambda handler for the updating trap status.
    :param event:
    :param context:
    :return:
    """

    messages_to_send = list()

    print("Compiling messages...")
    # Iterate through all traps data was sent for
    for trap_id in event['traps']:

        # Get JSON data for this trap
        trap_data = event['traps'][trap_id]
        hammer_down = trap_data['hammerDown']
        battery_level = trap_data['batteryLevel']

        # Get previous trap data from the database
        old_trap_data = database.get_trap_data_by_id(trap_id)

        if old_trap_data is not None:

            # Get data from old database information
            trap_name = old_trap_data.get('name', 'Unnamed')
            phone_numbers = old_trap_data.get('resident_phone_numbers', set())
            old_battery_level = old_trap_data.get('battery_level', 100)

            # Process deltas from previous data to send messages
            if hammer_down and not old_trap_data['hammer_down']:
                # Hammer has gone down
                messages_to_send.append((phone_numbers, f"{trap_name} trap triggered"))
                print(f"New message to send for trap '{trap_id}': Hammer switched to down")

            if battery_level <= LOW_BATTERY_LEVEL < old_battery_level:
                # Battery has fallen below low battery level
                messages_to_send.append((phone_numbers, f"{trap_name} battery level low"))
                print(f"New message to send for trap '{trap_id}': Battery level low")

            # Update trap in DB with new data
            database.update_trap(trap_id, hammer_status=hammer_down, battery_percent=battery_level)
        else:
            # Trap doesn't exist in DB, add it
            # TODO add new trap to db
            pass


    print("Sending messages...")
    # Iterate through all messages to send
    for phone_numbers, message in messages_to_send:
        # Iterate through all phone numbers for each message
        for phone_number in phone_numbers:
            # Send the message
            print(f"Sent to {phone_number}: {message}")
            twilio_interface.send_sms(phone_number, message)

