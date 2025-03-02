###############################################################################
#
# File: database.py
#
# Author: Isaac Ingram
#
# Purpose: Provide database functionality to the function handlers.
#
###############################################################################
import boto3
from typing import Union

dynamodb = boto3.resource('dynamodb')


traps_table: dynamodb.Table = dynamodb.Table('cs-mouse-traps')
registered_users_table: dynamodb.Table = dynamodb.Table('cs-mouse-registered-users')


def get_trap_data_by_id(trap_id: str):
    """
    Get trap data from the database by trap ID
    :param trap_id: String trap ID
    :return: The data, or None if no trap exists with this ID.
    """
    response = traps_table.get_item(Key={"trap_id": trap_id})
    return response.get("Item", None)


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
