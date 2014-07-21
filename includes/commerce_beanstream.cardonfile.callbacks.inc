<?php

/**
* Card on File callback: create new card
*
* @param array $form
* @param array $form_state
* @param object $payment_method
* @param array $card
*
* @return array Profile info, with at least a 'remote_id' key
*/
function commerce_beanstream_cardonfile_create($form, $form_state, $payment_method, $card) {
  drupal_set_message("cardonfile_create callback");
  dd($form, "cardonfile_create form");
  dd($form_state, "cardonfile_create form_state");
  dd($payment_method, "cardonfile_create payment_method");
  dd($card, "cardonfile_create card");
  $u = user_load($card->uid);
  $p = commerce_customer_profile_load_multiple(array(), array('uid' => $card->uid));
  return array('remote_id' => 1);
}

/**
* Card on file callback: background charge payment
*
* @param object $payment_method
*  The payment method instance definition array.
* @param object $card_data
*   The stored credit card data array to be processed
* @param object $order
*   The order object that is being processed
* @param array $charge
*   The price array for the charge amount with keys of 'amount' and 'currency'
*   If null the total value of the order is used.
*
* @return bool TRUE if the transaction was successful, otherwise FALSE
*/
function commerce_beanstream_cardonfile_charge($payment_method, $card_data, $order, $charge = NULL) {
  $url = $payment_method['settings']['payment_url'];

  // Format order total for transaction.
  if (!isset($charge)) {
    $wrapper = entity_metadata_wrapper('commerce_order', $order);
    $charge = commerce_line_items_total($wrapper->commerce_line_items);
  }

  //transaction info
  $transaction = commerce_payment_transaction_new('commerce_beanstream_payment', $order->order_id);
  $transaction->instance_id = $payment_method['instance_id'];
  $transaction->amount = $charge['amount'];
  $transaction->currency_code = $charge['currency_code'];
  $transaction->message = 'Name: @name';

  //build the request
  $request = array(
    'trnOrderNumber' => $order->order_id,
    'trnAmount' => number_format(commerce_currency_amount_to_decimal($transaction->amount,$transaction->currency_code),2),
    'customerCode' => $card_data->remote_id,
  );

  $bill_details = _commerce_beanstream_order_billing_details($payment_method, $order, $user, $request);
  if (empty($bill_details['ordName'])) {
    drupal_set_message(t('A billing address must be available to perform a transaction'), 'error');
    return FALSE;
  }
  $request = array_merge($request, $bill_details);

  $response = commerce_beanstream_request($payment_method, $request);

  //if declined set the tranaction status to false
  if($response['trnApproved'] != '1'  || empty($response['trnApproved'])){
    $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
  } else {
    //otheriwse set transaction status appropriately
    // Set the transaction status based on the type of transaction this was.
    switch ($payment_method['settings']['txn_type']) {
      case COMMERCE_CREDIT_AUTH_ONLY:
        $transaction->status = COMMERCE_PAYMENT_STATUS_PENDING;
        break;

      case COMMERCE_CREDIT_AUTH_CAPTURE:
        $transaction->status = COMMERCE_PAYMENT_STATUS_SUCCESS;
        break;
    }
  }

  //set tranaction variables to those returned from Beanstream
  $transaction->remote_id = $response['trnId'];
  $transaction->remote_status = $response['paymentMethod'];
  $transaction->payload[REQUEST_TIME] = $response;

  //set a msg to return to user
  $message = 'Your transacton has been <b>' . $response['messageText'] . '</b>';

  $transaction->message = $message;

  // save the transaction
  commerce_payment_transaction_save($transaction);

  // if the transaction is declined return false send error to user
  if($response['trnApproved'] != '1'){
    drupal_set_message(t('We received the following error processing your card. Please enter your information again or try a different card.'), 'error');
    drupal_set_message(check_plain($response['messageText']), 'error');
    return FALSE;
  }

  return TRUE;
}

/**
* Card on file callback: update card information
*
* @param array $form
*   The payment form
* @param array $form_state
*   The payment form state
* @param object $payment_method
*   The payment method instance definition array
* @param object $card_data
*   The new credit card data array to be processed
*/
function commerce_beanstream_cardonfile_update($form, &$form_state, $payment_method, $card_data){
  $cards = _commerce_beanstream_cardonfile_getcards($payment_method, $card_data->remote_id);

  foreach ($cards as $card) {
    if (($card['owner'] === $card_data->card_name) && ($card['number'] === $card_data->card_number)) {

      $data = array(
        'customerCode' => $card_data->remote_id,
        'cardId' => $card['cardId'],
        'trnExpYear' => substr($card_data->card_exp_year, -2),
        'trnExpMonth' => $card_data->card_exp_month,
      );

      $result = _commerce_beanstream_payment_profile_request('MODIFY_CARD', $payment_method, $data);
      if (!empty($result) && $result['responseCode'] === "1") {
        return TRUE;
      } else {
        return FALSE;
      }
    }
  }
}

/**
* Card on file callback: delete card information
*
* @param array $form
*   The payment form
* @param array $form_state
*   The payment form state
* @param object $payment_method
*   The payment method instance definition array
* @param object $card_data
*   The credit card data to be deleted
*/
function commerce_beanstream_cardonfile_delete($form, &$form_state, $payment_method, $card_data){
  $cards = _commerce_beanstream_cardonfile_getcards($payment_method, $card_data->remote_id);

  foreach ($cards as $card) {
    // TODO: Should check expiry and cardType as well here..
    if (($card['owner'] === $card_data->card_name) && ($card['number'] === $card_data->card_number)) {
      $data = array(
        'customerCode' => $card_data->remote_id,
        'cardId' => $card['cardId'],
      );

      $result = _commerce_beanstream_payment_profile_request_xml('DELETE_CARD', $payment_method, $data);

      if (!empty($result) && $result->responseCode == "1") {
        return TRUE;
      } else {
        return FALSE;
      }
    }
  }

  return FALSE;
}

/**
 * Helper function to lookup a list of cards on file at Beanstream.
 *
 * @param $payment_method Array The payment method instance definition array
 * @param $code String The customer code to lookup
 * @param null $orderNum String The unique order number for this transaction
 * @return array|bool List of cards, or FALSE on error/no cards on file.
 */
function _commerce_beanstream_cardonfile_getcards($payment_method, $code, $orderNum = NULL) {
  $data = array(
    'customerCode' => $code,
  );

  if (isset($orderNum)) {
    $data['trnOrderNumber'] = $orderNum;
  }

  $result = _commerce_beanstream_payment_profile_request_xml('GET_CARDS', $payment_method, $data);

  if (!empty($result) && $result->responseCode == "1") {
    $cards = array();
    foreach ($result->cardNode as $card) {
      $cards[] = array(
        'cardId' => $card->cardId->__toString(),
        'owner' => $card->owner->__toString(),
        'number' => substr($card->number->__toString(), -4),
        'expiry' => $card->expiry->__toString(),
      );
    }
    return $cards;
  } else {
    return FALSE;
  }
}

