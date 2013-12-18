<?php 

function commerce_beanstream_void_form($form, &$form_state, $order, $transaction) {
  // Load and store the payment method instance for this transaction.
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;


  $form['markup'] = array(
    '#markup' => t('Are you sure that you want to void this transaction?  '),
  );  

  $form = confirm_form($form,
    t('Are you sure that you want to void this transaction?'),
    'admin/commerce/orders/payment',
    t('You are permitted to void the transaction for 24hrs.'), 
    t('Void'),
    t('Cancel'),
    'confirm'
  );  

  return $form;
}

/**
 * Submit handler: process the void request.
 */
function commerce_beanstream_void_form_submit($form, &$form_state) {

  $transaction = $form_state['transaction'];

 // dd($transaction, "TRANSACTION");

  // Build a name-value pair array for this transaction.
  $nvp = array(
    'x_type' => 'VOID',
    'x_trans_id' => $transaction->remote_id,
  );

  // Submit the request to Authorize.Net.
/*  $response = commerce_authnet_aim_request($form_state['payment_method'], $nvp);

  // Update and save the transaction based on the response.
 // $transaction->payload[REQUEST_TIME] = $response;

  // If we got an approval response code...
  if ($response[0] == 1) {
    drupal_set_message(t('Transaction successfully voided.'));

    // Set the remote and local status accordingly.
    $transaction->status = COMMERCE_PAYMENT_STATUS_FAILURE;
    $transaction->remote_status = $response[11];

    // Update the transaction message to show that it has been voided.
    $transaction->message .= '<br />' . t('Voided: @date', array('@date' => format_date(REQUEST_TIME, 'short')));
  }
  else {
    drupal_set_message(t('Void failed: @reason', array('@reason' => check_plain($response[3]))), 'error');
  }

  commerce_payment_transaction_save($transaction);

  $form_state['redirect'] = 'admin/commerce/orders/' . $form_state['order']->order_id . '/payment';
*/

}


