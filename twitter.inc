<?php
// $Id: twitter.inc,v 1.9.2.6 2010/02/16 00:51:17 walkah Exp $

/**
 * Twitter API functions
 */

/**
 * Connect to the API using the 'proper' version (Oauth vs. standard)
 */
function twitter_connect($account) {
  module_load_include('lib.php', 'twitter');
  $auth = $account->get_auth();
  if (_twitter_use_oauth() && $auth['oauth_token'] && $auth['oauth_token_secret']) {
    module_load_include('lib.php', 'oauth_common');
    return new TwitterOAuth(variable_get('twitter_consumer_key', ''), variable_get('twitter_consumer_secret', ''), $auth['oauth_token'], $auth['oauth_token_secret']);
  }
  else if ($auth['password']) {
    return new Twitter($account->screen_name, $auth['password']);
  }
  else {
    return new Twitter;
  }
}

/**
 * Saves a TwitterUser object to {twitter_account}
 */
function twitter_account_save($twitter_user, $save_auth = FALSE, $account = NULL) {
  $values = (array) $twitter_user;
  $values['twitter_uid'] = $values['id'];

  if ($save_auth) {
    $values += $twitter_user->get_auth();
    if (empty($account)) {
      global $user;
      $account = $user;
    }
    $values['uid'] = $account->uid;
  }
  if (db_query("SELECT 1 FROM {twitter_account} WHERE twitter_uid = :twitter_uid", array(':twitter_uid' => $values['id']))->fetchField()) {
    drupal_write_record('twitter_account', $values, array('twitter_uid'));
  }
  else {
    drupal_write_record('twitter_account', $values);
  }
}

/**
 * Load a Twitter account from {twitter_account}.
 *
 * @param $id
 *   Twitter UID
 *
 * @return 
 *   TwitterUser object
 *
 * @todo How well does this handle the Twitter UID not being in the
 *       DB?
 */
function twitter_account_load($id) {
  module_load_include('lib.php', 'twitter');

  $values = db_fetch_array(db_query("SELECT * FROM {twitter_account} WHERE twitter_uid = :twitter_uid", array(':twitter_uid' => $id)));
  $values['id'] = $values['twitter_uid'];
  $account = new TwitterUser($values);
  $account->set_auth($values);
  $account->uid = $values['uid'];
  $account->import = $values['import'];
  $account->is_global = $values['is_global'];

  return $account;
}

/**
 * Saves a TwitterStatus object to {twitter}
 */
function twitter_status_save($status) {
  $status->twitter_id = $status->id;
  $status->screen_name = $status->user->screen_name;
  $status->created_time = strtotime($status->created_at);

  if (db_query("SELECT 1 FROM {twitter} WHERE twitter_id = :twitter_id", array(':twitter_id' => $status->id))->fetchField()) {
    drupal_write_record('twitter', $status, array('twitter_id'));
  }
  else {
    drupal_write_record('twitter', $status);
  }

}

/**
 * Post a message to twitter
 */
function twitter_set_status($twitter_account, $status) {
  module_load_include('lib.php', 'twitter');

  $twitter = twitter_connect($twitter_account);
  $twitter->status_update($status);
}

/**
 * Fetches a user's timeline
 */
function twitter_fetch_user_timeline($id) {
  $account = twitter_account_load($id);

  $since = db_query("SELECT MAX(twitter_id) FROM {twitter} WHERE screen_name = :screen_name", array(':screen_name' => $account->screen_name))->fetchField();

  $twitter = twitter_connect($account);

  $params = array();
  if ($since) {
    $params['since_id'] = $since;
  }

  $statuses = $twitter->user_timeline($account->id, $params, $account->protected);
  foreach ($statuses as $status) {
    twitter_status_save($status);
  }

  if (count($statuses) > 0) {
    twitter_account_save($statuses[0]->user);
  }
  // TODO Please review the conversion of this statement to the D7 database API syntax.
  /* db_query("UPDATE {twitter_account} SET last_refresh = %d WHERE twitter_uid=%d", REQUEST_TIME, $account->id) */
  db_update('twitter_account')
  ->fields(array(
    'last_refresh' => REQUEST_TIME,
  ))
  ->condition('twitter_uid', $account->id)
  ->execute();
}

/**
 * Delete a user from {twitter_account}.
 *
 * @param $twitter_uid
 *   An integer with the Twitter UID.
 *
 * @param $screen_name
 *   Optional string with the user name.
 */
function twitter_user_delete($twitter_uid, $screen_name = NULL) {
  $sql = "DELETE FROM {twitter_account} WHERE twitter_uid = %d";
  $args = array($twitter_uid);
  if (!empty($screen_name)) {
    $sql .= " AND screen_name = '%s'";
    $args[] = $screen_name;
  }
  db_query($sql, $args);
}
